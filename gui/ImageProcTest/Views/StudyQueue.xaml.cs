using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using ImageProcTest.Models;
using ImageProcTest.ViewModels;

namespace ImageProcTest.Views;

public partial class StudyQueue : System.Windows.Controls.UserControl
{
    public StudyQueue()
    {
        InitializeComponent();
    }

    private void OnFilterChanged(object sender, RoutedEventArgs e)
    {
        if (sender is not RadioButton rb || StudyListBox.ItemsSource is null) return;

        var view = CollectionViewSource.GetDefaultView(StudyListBox.ItemsSource);
        if (view is null) return;

        if (rb == FilterAll || rb.Content?.ToString() == "All")
        {
            view.Filter = null;
        }
        else if (rb.Content?.ToString() == "Queued")
        {
            view.Filter = o => o is StudyEntry s && s.Status == StudyStatus.Queued;
        }
        else if (rb.Content?.ToString() == "Done")
        {
            view.Filter = o => o is StudyEntry s && s.Status != StudyStatus.Queued;
        }
    }

    private void OnStudySelected(object sender, SelectionChangedEventArgs e)
    {
        if (StudyListBox.SelectedValue is string studyId && DataContext is MainWindowViewModel vm)
        {
            vm.ActiveStudyId = studyId;
        }
    }
}
