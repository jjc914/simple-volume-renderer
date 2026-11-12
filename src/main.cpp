#include "application.hpp"

int main() {
    Application app;
    if (!app.Init(1400, 800, "Simple Volume Renderer")) return -1;
    app.Run();
    return 0;
}
