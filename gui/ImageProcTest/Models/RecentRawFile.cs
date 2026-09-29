using System.Windows.Input;

namespace ImageProcTest.Models;

/// <summary>
/// One entry of the Open Recent submenu (#225 row 1, GUI-C-160).
///
/// <para>The command travels ON the item rather than being looked up from the window's DataContext.
/// A submenu's items live in a popup, so a <c>RelativeSource AncestorType=Window</c> binding there
/// resolves against a different visual tree and silently produces no command — an entry that looks
/// right and does nothing. Carrying the command makes that failure impossible to write.</para>
/// </summary>
/// <param name="Path">Full path of the raw file, shown as the menu header.</param>
/// <param name="OpenCommand">Command that loads <paramref name="Path"/>.</param>
public sealed record RecentRawFile(string Path, ICommand OpenCommand);
