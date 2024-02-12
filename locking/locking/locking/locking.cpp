// Race conditions example
// Adam Sampson <a.sampson@abertay.ac.uk>

#include <iostream>
#include <string>

#include "account.h"

// Import things we need from the standard library
using std::cout;
using std::endl;

Account bill;

int main(int argc, char *argv[])
{
	cout << "Initial: " << bill.total() << "\n";

	bill.add(4, 17);
	bill.add(3, 0);
	bill.add(10, 99);

	cout << "Total: " << bill.total() << "\n";

	return 0;
}