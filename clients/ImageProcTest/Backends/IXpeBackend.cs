namespace ImageProcTest
{
    internal interface IXpeBackend
    {
        BackendHealthResult CheckHealth();

        void Shutdown();
    }
}
