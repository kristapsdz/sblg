#include <unistd.h>

int
main(void)
{

	if (-1 == pledge("stdio", NULL))
		return(1);

	return(0);
}
