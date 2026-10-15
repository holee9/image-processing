// AC-8: Mock backend never active in integration tests.
using System.Reflection;
using ImageProcTest.IntegrationTests.Fixtures;

namespace ImageProcTest.IntegrationTests.Safety;

/// <summary>
/// Verifies that MockXpeBackend and CompositeXpeBackend are not loaded into
/// the integration test AppDomain or used as a fallback path.
/// Covers REQ-GUI-IT-007, AC-8.
/// </summary>
[Trait("Category", "Safety")]
public sealed class MockBlockingTests
{
    /// <summary>
    /// REQ-GUI-IT-007: MockXpeBackend type must not be resolvable from the test assembly
    /// or any currently-loaded assembly.
    /// </summary>
    [Fact]
    public void MockXpeBackend_TypeNotLoadedInTestAssemblies()
    {
        var mockType = AppDomain.CurrentDomain
            .GetAssemblies()
            .SelectMany(SafeGetTypes)
            .FirstOrDefault(t => t.Name == "MockXpeBackend");

        Assert.Null(mockType);
    }

    /// <summary>
    /// REQ-GUI-IT-007: CompositeXpeBackend type must not be resolvable in the test session.
    /// </summary>
    [Fact]
    public void CompositeXpeBackend_TypeNotLoadedInTestAssemblies()
    {
        var compositeType = AppDomain.CurrentDomain
            .GetAssemblies()
            .SelectMany(SafeGetTypes)
            .FirstOrDefault(t => t.Name == "CompositeXpeBackend");

        Assert.Null(compositeType);
    }

    /// <summary>
    /// REQ-GUI-IT-007: Test project itself must not reference any "Mock" in its type graph.
    /// </summary>
    [Fact]
    public void TestAssembly_HasNoMockBackendReference()
    {
        var asm = typeof(MockBlockingTests).Assembly;
        var hasMockRef = asm.GetReferencedAssemblies()
            .Any(r => r.Name?.Contains("Mock", StringComparison.OrdinalIgnoreCase) == true);

        Assert.False(hasMockRef, "Test assembly must not reference any Mock assembly.");
    }

    /// <summary>
    /// GUI-C-208 (D4): the control for the two scans above. <see cref="SafeGetTypes"/> swallows a <see cref="ReflectionTypeLoadException"/> and returns nothing, so a scan that silently saw
    /// nothing would make <c>Assert.Null</c> true for any type name. This one asks the same scan for a type that IS loaded.
    /// (D4 as first reported said CompositeXpeBackend exists nowhere and its test could never fail. That was repeated from SPEC §4's 2026-09-17 note without a search: the type
    /// exists, as clients/ImageProcTest/Backends/CompositeXpeBackend.cs, used by the legacy clients app. The test is a real guard and stays.)
    /// </summary>
    [Fact]
    public void TheTypeScan_FindsATypeThatIsLoaded()
    {
        var found = AppDomain.CurrentDomain
            .GetAssemblies()
            .SelectMany(SafeGetTypes)
            .FirstOrDefault(t => t.Name == nameof(MockBlockingTests));

        Assert.NotNull(found);
    }

    private static IEnumerable<Type> SafeGetTypes(Assembly asm)
    {
        try { return asm.GetTypes(); }
        catch (ReflectionTypeLoadException) { return Array.Empty<Type>(); }
    }
}
