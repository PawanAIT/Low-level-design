#include <iostream> // Standard library for Input/Output (cout, cin)
using namespace std;

class A {
private:
    int x;
public:
    explicit A(int x) : x(x) {}
    
    void print() {
        cout << x << "\n";
    }
};

int main() {
    A a(5);
    a.print();
    return 0;
}
