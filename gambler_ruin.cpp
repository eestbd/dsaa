#include<iostream>
#include<cstdlib>
#include<ctime>
using namespace std;

int main() {
	int stake, goal, trials, wins = 0;
        cout << "Put stake, goal and trials\n";
        cin >> stake >> goal >> trials;

        srand(time(NULL));
	for (int i = 0; i < trials; i++) {
		int t = stake;
		while (t > 0 && t < goal)
			if (rand() % 2 == 1) t++;
			else t--;
		if (t == goal) wins++;
	}
	cout << wins << " wins of " << trials << endl;
}
