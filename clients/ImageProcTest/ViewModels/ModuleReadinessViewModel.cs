using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace ImageProcTest.ViewModels
{
    internal sealed class ModuleReadinessViewModel : INotifyPropertyChanged
    {
        private string summaryText = "Module readiness: not checked";

        public event PropertyChangedEventHandler? PropertyChanged;

        public ObservableCollection<ModuleReadinessSnapshot> Modules { get; } = [];

        public string SummaryText
        {
            get => summaryText;
            private set => SetField(ref summaryText, value);
        }

        public IReadOnlyList<ModuleReadinessSnapshot> Refresh(BackendHealthResult? commonHealth)
        {
            var modules = ModuleReadinessService.Evaluate(commonHealth);
            Modules.Clear();
            foreach (var module in modules)
            {
                Modules.Add(module);
            }

            SummaryText = BuildSummary(modules);
            return modules;
        }

        private static string BuildSummary(IReadOnlyList<ModuleReadinessSnapshot> modules)
        {
            if (modules.Count == 0)
            {
                return "Module readiness: no modules registered.";
            }

            var enabledCount = modules.Count(module => module.ProcessingEnabled);
            var levels = modules
                .GroupBy(module => module.Level)
                .OrderBy(group => ModuleLevelSortKey(group.Key))
                .Select(group => $"{group.Key}={group.Count()}");
            var offModules = modules
                .Where(module => !module.ProcessingEnabled && module.ModuleName != "xpe_common")
                .Select(module => $"{module.ModuleName}:{module.Level}");

            return $"Executable modules={enabledCount}; levels {string.Join(", ", levels)}; " +
                $"Off modules={string.Join(", ", offModules)}. Missing DLLs and future modules are skipped by graceful degradation.";
        }

        private static int ModuleLevelSortKey(string level)
        {
            if (string.IsNullOrWhiteSpace(level) ||
                !level.StartsWith("R", StringComparison.OrdinalIgnoreCase))
            {
                return 0;
            }

            return int.TryParse(level[1..], out var rank) ? rank : 0;
        }

        private bool SetField<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
        {
            if (Equals(field, value))
            {
                return false;
            }

            field = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
            return true;
        }
    }
}
