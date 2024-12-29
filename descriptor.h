#include <iostream>

using namespace std;

class ResourceDescriptors {
    private:
     int NUM_RESOURCES = 10;

    public:
    int availableResources[10];
    int allocatedResources[19][10];
    int requestedResources[19][10];
    
    ResourceDescriptors(){
        for(int i = 0; i < 10; i++){
            availableResources[i] = 20;
        }

        for(int i = 0; i < 19; i++){
            for(int j = 0; j < 10; j++){
                allocatedResources[i][j] = 0;
                requestedResources[i][j] = 0;
            }
        }
    }

    void printTables(){
        cout << "Available resources:" << endl;
        cout << "r0\tr1\tr2\tr3\tr4\tr5\tr6\tr7\tr8\tr9" << endl;
        for(int i = 0; i < 10; i++){
            cout << availableResources[i]<< "\t";
        }
        cout << endl;

        cout << "allocatedResources: " << endl;
        cout << "\tr0\tr1\tr2\tr3\tr4\tr5\tr6\tr7\tr8\tr9" << endl;
        for(int i = 0; i < 19; i++){
            cout << "process " << i << " ";
            for(int j = 0; j < 10; j++){
                cout << allocatedResources[i][j] << "\t";
            }
            cout << endl;
        }
        cout << "requestedResources: " << endl;
        cout << "\tr0\tr1\tr2\tr3\tr4\tr5\tr6\tr7\tr8\tr9" << endl;
        for(int i = 0; i < 19; i++){
            cout << "process " << i << " ";
            for(int j = 0; j < 10; j++){
                cout << requestedResources[i][j] << "\t";
            }
            cout << endl;
        }
    }
     void printResources(){
        cout << "Available resources:" << endl;
        cout << "r0\tr1\tr2\tr3\tr4\tr5\tr6\tr7\tr8\tr9" << endl;
        for(int i = 0; i < 10; i++){
            cout << availableResources[i]<< "\t";
        }
        cout << endl;
    }
};



