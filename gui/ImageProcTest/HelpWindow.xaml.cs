using System.IO;
using System.Windows;

namespace ImageProcTest;

public partial class HelpWindow : Window
{
    public HelpWindow(string helpTitle, string documentPath)
    {
        InitializeComponent();
        HelpTitle = helpTitle;
        CurrentDocumentPath = documentPath;
        Title = helpTitle;
        DataContext = this;
        Loaded += OnLoaded;
    }

    public string HelpTitle { get; }

    public string CurrentDocumentPath { get; }

    public bool DocumentLoaded { get; private set; }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        try
        {
            if (!File.Exists(CurrentDocumentPath))
            {
                throw new FileNotFoundException("Help document does not exist.", CurrentDocumentPath);
            }

            HelpBrowser.Navigate(new Uri(CurrentDocumentPath));
            DocumentLoaded = true;
        }
        catch (Exception ex)
        {
            HelpErrorTextBlock.Text = ex.Message;
            HelpErrorOverlay.Visibility = Visibility.Visible;
            DocumentLoaded = false;
        }
    }
}
