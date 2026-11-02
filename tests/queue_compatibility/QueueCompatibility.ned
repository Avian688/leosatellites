simple QueueCompatibilityTest
{
    parameters:
        string expectedQueue = default("");
}

network QueueCompatibility
{
    submodules:
        test: QueueCompatibilityTest;
    connections allowunconnected:
}
