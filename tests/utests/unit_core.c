#include <tld.h>


int main(void)
{
        /* Tests for LOG_MSG and H1_MSG */
        LOG_MSG("This is a log message");
        H1_MSG("This is a H1 heading message");
        H2_MSG("This is a H2 heading message");
        H3_MSG("This is a H3 heading message");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;

}
