#include <temrixstd.h>
#include "Compositor.hpp"
#include "GpuDevice.hpp"

int main(int argc, char **argv)
{
    String::Print("[compositor] starting\n");

    Compositor compositor;
    if (!compositor.Initialize())
    {
        String::Print("[compositor] Initialize failed\n");
        return -1;
    }

    String::Print("[compositor] initialized, running\n");

    compositor.Run();

    return 0;
}