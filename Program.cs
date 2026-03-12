using Libs;
using Microsoft.Extensions.DependencyInjection;

namespace Client
{
    internal static class Program
    {
        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            // Настройка контейнера DI
            var services = new ServiceCollection();
            ConfigureServices(services);

            // Создание провайдера услуг
            var serviceProvider = services.BuildServiceProvider();

            // Запуск главной формы
            var form = serviceProvider.GetRequiredService<ClientForm>();
            // To customize application configuration such as set high DPI settings or default font,
            // see https://aka.ms/applicationconfiguration.
            //ApplicationConfiguration.Initialize();
            Application.Run(form);
        }
        private static void ConfigureServices(IServiceCollection services)
        {
            // Регистрация зависимостей
            services.AddTransient<ILogic, Logic>();
            services.AddTransient<IDS, DS>(); // Регистрируем интерфейс и его реализацию
            services.AddTransient<ClientForm>(); // Регистрируем форму
        }
    }
}