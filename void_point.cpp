#include <iostream>
using namespace std;

void increase (void* data, int pSize) {
	if(pSize == sizeof(char)) {
		char* pChar;
		pChar = (char*)data;
		++(*pChar);
	} else if(pSize == sizeof(int)) {
		int* pInt;
		pInt = (int*)data;
		++(*pInt);
	}
}

int main() {
	char a = 'x';
	int b = 1602;
	increase (&a, sizeof(a));
	increase (&b, sizeof(b));
	cout << a << ", " << b << '\n';
	return 0;
}
