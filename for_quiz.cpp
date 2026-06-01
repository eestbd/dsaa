#include <iostream>
// #include <unistd.h>
using namespace std;

int main(){
	int f = 0, g = 1;
	for (int i = 0; i <= 10; i++) {
		cout << f << "\n";
		// sleep(3);
		f = f + g;
		g = f - g;
	}
}

