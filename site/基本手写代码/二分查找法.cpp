/**************************************************************************
**	���ֲ��ҷ�
**	�����������в���target�����ҵ�����index�����򷵻�-1
*************************************************************************/
#include <iostream>
using namespace std;

int binarySearch(int arr[], int n, int target){

	//��[l,r]�в���target
	//�����ұ߽�
	int l = 0, r = n - 1;
	int res;
	while (l <= r){
		//int mid = (l + r) / 2;
		//��ֹ���
		int mid = l + (r - l) / 2;
		if (arr[mid] == target){
			res = mid;
			break;
		}
		//��[l,mid-1]�в���target
		else if (arr[mid] > target)
			r = mid - 1;
		//��[mid+1,r]�в���target
		else
			l = mid + 1;
	}
	return res;
}


int main(){

	int a[6] = { 0, 1, 2, 3, 4, 5 };
	int res = binarySearch(a, 6, 4);
	cout << res << endl;
	system("pause");
	return 0;
}