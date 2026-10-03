using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-219c (Codex #112): a private copy of the DLLs the synthetic-oracle worker will load, made BEFORE the verdict is asked for, hashed AFTER it was copied, and handed to the worker in
    /// place of the originals. The bytes that are hashed and the bytes that are judged are then the same bytes by construction: nothing that happens to the files in the application folder
    /// while the oracle runs (a replacement, A to B and back to A) can reach the worker, so no "hash before, hash after, run again" is needed. The copy is removed when the verdict is stored.
    ///
    /// What is copied: the preprocess DLL, the dependency DLLs the application's own loader names for it, and every DLL those import (read from the PE import tables, transitively) that can
    /// be found in the same places the loader looks in. The identity of a snapshot is the list of those names with the SHA-256 of each copy, so a changed dependency is a changed subject
    /// (Codex #112 observed that the key used to cover <c>xpe_preprocess.dll</c> only).
    /// </summary>
    internal sealed class PreprocessOracleSnapshot : IDisposable
    {
        private const int MaxFiles = 64;
        private const string DirectoryPrefix = "xpe-oracle-snap-";

        private PreprocessOracleSnapshot(string? folder, string dllPath, string identity, IReadOnlyList<string> files)
        {
            Folder = folder;
            DllPath = dllPath;
            Identity = identity;
            Files = files;
        }

        /// <summary>The private folder the copies are in (null when there was nothing to copy).</summary>
        public string? Folder { get; }

        /// <summary>The preprocess DLL to hand to the worker: the copy, or the original path when the original does not exist (the oracle then says so itself).</summary>
        public string DllPath { get; }

        /// <summary>Names with the SHA-256 of each copied file, sorted: <c>fmt.dll=…;xpe_common.dll=…;xpe_preprocess.dll=…</c>, or <c>missing</c>.</summary>
        public string Identity { get; }

        /// <summary>The file names that were copied.</summary>
        public IReadOnlyList<string> Files { get; }

        /// <summary>
        /// Copies, then hashes the copies. <paramref name="afterCopy"/> is a test seam called once the files are copied and before they are hashed (a slow disk, or a file changing at the worst
        /// moment, can be put there deterministically).
        /// </summary>
        public static PreprocessOracleSnapshot Create(string dllPath, Action<string>? afterCopy = null)
        {
            if (!File.Exists(dllPath))
            {
                return new PreprocessOracleSnapshot(null, dllPath, "missing", []);
            }

            var directory = Path.Combine(Path.GetTempPath(), $"{DirectoryPrefix}{Environment.ProcessId}-{Guid.NewGuid():N}");
            System.IO.Directory.CreateDirectory(directory);
            var copied = new List<string>();
            try
            {
                var mainFileName = Path.GetFileName(dllPath);
                foreach (var (name, source) in Closure(dllPath))
                {
                    if (CopyShared(source, Path.Combine(directory, name)))
                    {
                        copied.Add(name);
                    }
                    else if (!string.Equals(name, mainFileName, StringComparison.OrdinalIgnoreCase))
                    {
                        // GUI-C-219d (Codex #116): a dependency that was found and then could not be copied (removed in between) means the set that would be judged is not the set
                        // that is installed; the check fails closed (a setup failure, which is a verdict) instead of judging a smaller set.
                        throw new FileNotFoundException("A DLL the preprocess DLL depends on disappeared while it was being copied: " + name, source);
                    }
                }

                afterCopy?.Invoke(dllPath);

                var mainName = Path.GetFileName(dllPath);
                if (!copied.Contains(mainName, StringComparer.OrdinalIgnoreCase))
                {
                    // the preprocess DLL itself could not be read (removed between the existence check and the copy): there is nothing to judge
                    Discard(directory);
                    return new PreprocessOracleSnapshot(null, dllPath, "missing", []);
                }

                var identity = string.Join(";", copied
                    .OrderBy(n => n, StringComparer.OrdinalIgnoreCase)
                    .Select(n => n + "=" + Sha256Of(Path.Combine(directory, n))));
                return new PreprocessOracleSnapshot(directory, Path.Combine(directory, mainName), identity, copied);
            }
            catch
            {
                Discard(directory);
                throw;
            }
        }

        /// <summary>
        /// GUI-C-219d: the identity (the same format as <see cref="Identity"/>) of the files as they are on disk NOW, hashed in place, with no copy. It is what the processing commands compare with the
        /// identity a verdict was made for just before they run. "missing" when the preprocess DLL does not exist. Throws when a file in the set cannot be read (the caller treats that as "not the same").
        /// </summary>
        public static string IdentityOfOriginals(string dllPath)
        {
            if (!File.Exists(dllPath))
            {
                return "missing";
            }

            return string.Join(";", Closure(dllPath)
                .OrderBy(f => f.Name, StringComparer.OrdinalIgnoreCase)
                .Select(f => f.Name + "=" + Sha256Of(f.Source)));
        }

        public void Dispose()
        {
            if (Folder is not null)
            {
                Discard(Folder);
            }
        }

        /// <summary>Best-effort removal of snapshot folders a process that ended abruptly left behind (never one of a process that is still running).</summary>
        public static void ReclaimOldFolders()
        {
            try
            {
                foreach (var folder in System.IO.Directory.EnumerateDirectories(Path.GetTempPath(), DirectoryPrefix + "*"))
                {
                    var owner = Path.GetFileName(folder)[DirectoryPrefix.Length..].Split('-')[0];
                    if (int.TryParse(owner, out var pid) && (pid == Environment.ProcessId || IsRunning(pid)))
                    {
                        continue;
                    }

                    Discard(folder);
                }
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                // reclaiming is a courtesy to the temp folder, never a reason to fail a check
            }
        }

        private static bool IsRunning(int pid)
        {
            try
            {
                using var process = System.Diagnostics.Process.GetProcessById(pid);
                return !process.HasExited;
            }
            catch (ArgumentException)
            {
                return false;
            }
        }

        // ------------------------------------------------------------------------------------------------------------------------------ what is copied

        /// <summary>The DLLs that matter, as (file name, where it was found): the preprocess DLL, the loader's named dependencies, and the closure of their PE imports found beside them.</summary>
        internal static IReadOnlyList<(string Name, string Source)> Closure(string dllPath)
        {
            var directories = NativeDependencyLoader.DependencyDirectoriesOf(dllPath).ToList();
            var found = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var pending = new Queue<string>();

            void Want(string name)
            {
                if (found.ContainsKey(name) || found.Count >= MaxFiles)
                {
                    return;
                }

                foreach (var directory in directories)
                {
                    var candidate = Path.Combine(directory, name);
                    if (File.Exists(candidate))
                    {
                        found[name] = candidate;
                        pending.Enqueue(candidate);
                        return;
                    }
                }
            }

            found[Path.GetFileName(dllPath)] = dllPath;
            pending.Enqueue(dllPath);
            foreach (var dependency in NativeDependencyLoader.DependenciesOf(Path.GetFileName(dllPath)))
            {
                Want(dependency);
            }

            while (pending.Count > 0)
            {
                foreach (var import in PeImports.Of(pending.Dequeue()))
                {
                    Want(import);
                }
            }

            return found.Select(p => (p.Key, p.Value)).ToList();
        }

        private static bool CopyShared(string source, string destination)
        {
            try
            {
                using var from = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                using var to = new FileStream(destination, FileMode.CreateNew, FileAccess.Write, FileShare.None);
                from.CopyTo(to);
                return true;
            }
            catch (Exception ex) when (ex is FileNotFoundException or DirectoryNotFoundException)
            {
                return false;
            }
        }

        private static string Sha256Of(string path)
        {
            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            return Convert.ToHexString(SHA256.HashData(stream));
        }

        private static void Discard(string directory)
        {
            try
            {
                System.IO.Directory.Delete(directory, recursive: true);
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or DirectoryNotFoundException)
            {
                // a copy the worker still holds open is left for ReclaimOldFolders
            }
        }
    }

    /// <summary>The DLL names a PE file imports (its import directory): enough to know which other DLLs of ours a module pulls in. Not a general PE reader.</summary>
    internal static class PeImports
    {
        public static IReadOnlyList<string> Of(string path)
        {
            try
            {
                using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                using var reader = new BinaryReader(stream);
                return Read(reader);
            }
            catch (Exception ex) when (ex is IOException or EndOfStreamException or InvalidDataException or UnauthorizedAccessException)
            {
                return [];
            }
        }

        private static List<string> Read(BinaryReader r)
        {
            var names = new List<string>();
            r.BaseStream.Position = 0x3C;
            var peOffset = r.ReadInt32();
            if (peOffset <= 0 || peOffset > r.BaseStream.Length - 4)
            {
                return names;
            }

            r.BaseStream.Position = peOffset;
            if (r.ReadUInt32() != 0x00004550)   // "PE\0\0"
            {
                return names;
            }

            r.BaseStream.Position = peOffset + 6;
            var sectionCount = r.ReadUInt16();
            r.BaseStream.Position = peOffset + 20;
            var optionalSize = r.ReadUInt16();
            var optional = peOffset + 24;
            r.BaseStream.Position = optional;
            var magic = r.ReadUInt16();
            var directories = optional + (magic == 0x20B ? 112 : 96);
            r.BaseStream.Position = directories + 8;   // the import directory is data directory number 1
            var importRva = r.ReadUInt32();
            if (importRva == 0)
            {
                return names;
            }

            var sections = new List<(uint Rva, uint Size, uint Raw)>();
            r.BaseStream.Position = optional + optionalSize;
            for (var i = 0; i < sectionCount; i++)
            {
                r.BaseStream.Position += 8;
                var virtualSize = r.ReadUInt32();
                var virtualAddress = r.ReadUInt32();
                var rawSize = r.ReadUInt32();
                var rawPointer = r.ReadUInt32();
                sections.Add((virtualAddress, Math.Max(virtualSize, rawSize), rawPointer));
                r.BaseStream.Position += 16;
            }

            long ToOffset(uint rva)
            {
                foreach (var (va, size, raw) in sections)
                {
                    if (rva >= va && rva < va + size)
                    {
                        return rva - va + raw;
                    }
                }

                return -1;
            }

            var descriptor = ToOffset(importRva);
            for (var guard = 0; descriptor >= 0 && guard < 512; guard++, descriptor += 20)
            {
                r.BaseStream.Position = descriptor + 12;
                var nameRva = r.ReadUInt32();
                if (nameRva == 0)
                {
                    break;
                }

                var nameOffset = ToOffset(nameRva);
                if (nameOffset < 0)
                {
                    break;
                }

                r.BaseStream.Position = nameOffset;
                var bytes = new List<byte>();
                for (var b = r.ReadByte(); b != 0 && bytes.Count < 260; b = r.ReadByte())
                {
                    bytes.Add(b);
                }

                names.Add(System.Text.Encoding.ASCII.GetString(bytes.ToArray()));
            }

            return names;
        }
    }
}
