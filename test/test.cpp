#include <iostream>
using namespace std;

extern void print4();

int main()
{
    for (int i = 0; i < 5; i++) {
        cout << "it is begin" << endl;
        switch(i) {
        case 0:
            cout << "it is 0" << endl; break;
        case 1:
            cout << "it is 1" << endl; break;
        case 2:
            cout << "it is 2" << endl; break;
        case 3:
            cout << "it is 3" << endl; break;
        case 4:
            print4(); 
            break;
        default:
            cout << "wtf" << endl;
        }
        cout << "it is end" << endl;
    }

}