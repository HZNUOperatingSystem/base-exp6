#include "stat.h"
#include "types.h"
#include "user.h"

int main(void) {
    char* msg = "Goodbye!\n";
    write(1, msg, strlen(msg));
    shutdown();
    exit(1);
}
