#include <switch.h>

#include "ui/app.hpp"

int main(int, char**) {
    consoleInit(nullptr);
    int rc = ui::run();
    consoleExit(nullptr);
    return rc;
}
