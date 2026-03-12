using Libs;
using Microsoft.Extensions.DependencyInjection;

namespace DS_Server
{
    class Program
    {
        static async Task Main(string[] args)
        {
            // Настройка контейнера DI
            var services = new ServiceCollection();
            ConfigureServices(services);

            // Создание провайдера услуг
            var serviceProvider = services.BuildServiceProvider();

            // Получение экземпляра Server через DI
            var server = serviceProvider.GetRequiredService<Server>();

            // Запуск сервера
            await Server.Start();
        }

        private static void ConfigureServices(IServiceCollection services)
        {
            // Регистрация зависимостей
            services.AddTransient<IDS, DS>(); // Регистрируем интерфейс и его реализацию
            services.AddTransient<Server>(); // Регистрируем Server
        }
    }
}
