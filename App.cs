using System.Diagnostics.CodeAnalysis;
using Windows.ApplicationModel.Core;
using Windows.UI.Core;

namespace SpaghettiKartX
{
    /// <summary>
    /// Represent a non-XAML UWP app, i.e. an app without XAML content that is hosted in a <see cref="CoreWindow"/> instance.
    /// </summary>
    public sealed partial class App : IFrameworkViewSource, IFrameworkView
    {
        /// <summary>
        /// The <see cref="CoreApplicationView"/> for the current app instance.
        /// </summary>
        private CoreApplicationView? _applicationView;

        /// <summary>
        /// The <see cref="CoreWindow"/> used to display the app content.
        /// </summary>
        private CoreWindow? _window;

        /// <summary>
        /// The entry point for the application.
        /// </summary>
        public static void Main()
        {
            CoreApplication.Run(new App());
        }

        /// <inheritdoc/>
        public IFrameworkView CreateView()
        {
            return this;
        }

        /// <inheritdoc/>
        [MemberNotNull(nameof(_applicationView))]
        public void Initialize(CoreApplicationView applicationView)
        {
            this._applicationView = applicationView;
        }

        /// <inheritdoc/>
        [MemberNotNull(nameof(_window))]
        public void SetWindow(CoreWindow window)
        {
            this._window = window;
        }

        /// <inheritdoc/>
        public void Load(string entryPoint)
        {
        }

        /// <inheritdoc/>
        public void Run()
        {
            // Activate the current window
            _window!.Activate();

            // Process any messages in the queue
            _window.Dispatcher.ProcessEvents(CoreProcessEventsOption.ProcessUntilQuit);
        }

        /// <inheritdoc/>
        public void Uninitialize()
        {
        }
    }
}
