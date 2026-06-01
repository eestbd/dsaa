#include <iostream>
#include <unistd.h>
using namespace std;

int main() {
    cout << "Start\n";
    
    cout << "Waiting...";   // 줄바꿈 없어서 flush 안됨
    			    // 터미널은 line-buffered 환경이므로
    sleep(5);

    cout << " Done\n";
}
