#include <iostream>
#include <unistd.h>
using namespace std;

int main() {
    cout << "Start" << endl;

    cout << "Waiting..." << endl;  // flush 발생!
    sleep(5);

    cout << "Done" << endl;
}
