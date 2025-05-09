using Avalonia.Controls;

using AvaloniaEdit;
using AvaloniaEdit.TextMate;

using TextMateSharp.Grammars;

namespace Melchior.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();

        // Need a scheme for doing this non-hard-codedly
        SetTextEditor(ThemeName.LightPlus);
    }

    private void SetTextEditor(ThemeName theme)
    {
        // [glowysourworm] https://github.com/avaloniaui/avaloniaedit (AvaloniaEdit setup)

        //First of all you need to have a reference for your TextEditor for it to be used inside AvaloniaEdit.TextMate project.
        var _textEditor = this.FindControl<MainView>("TheMainView")
                              ?.FindControl<SimpleTextEditor>("TextEditor")
                              ?.FindControl<TextEditor>("Editor");

        if (_textEditor != null)
        {
            //Here we initialize RegistryOptions with the theme we want to use.
            var _registryOptions = new RegistryOptions(theme);

            //Initial setup of TextMate.
            var _textMateInstallation = _textEditor.InstallTextMate(_registryOptions);

            //Here we are getting the language by the extension and right after that we are initializing grammar with this language.
            //And that's all 😀, you are ready to use AvaloniaEdit with syntax highlighting!
            _textMateInstallation.SetGrammar(_registryOptions.GetScopeByLanguageId(_registryOptions.GetLanguageByExtension(".cs").Id));
        }
    }

    private void ThemeEditor_Click(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        var menuItem = sender as MenuItem;

        if (menuItem != null)
        {
            // Need a scheme for doing this non-hard-codedly
            SetTextEditor((ThemeName)menuItem.Tag);
        }
    }

    private void ThemeMain_Click(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        this.RequestedThemeVariant = (sender as MenuItem)?.Tag as Avalonia.Styling.ThemeVariant;
    }
}