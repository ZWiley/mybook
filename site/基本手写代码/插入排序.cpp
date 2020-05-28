//��������ƽ�����O(n^2),���O(n)���O(n^2),���ռ临�Ӷ�O(1)���ȶ�

/*
��������˼��
a[5] = {2,1,3,4,5}
���ѭ����a[0]���ùܣ�Ĭ�����򣬴�a[1]��ʼ����ѭ��
�ڲ�ѭ��������Ҫ�ƶ���Ԫ��a[1]������ʱ����
		����a[1]ǰ���Ԫ�أ��뵱ǰa[1]���бȽϣ��������ĵ�ǰԪ�ش���a[1]��Ԫ�غ���a[j] = a[j - 1]
		ֱ������Ԫ��С�ڵ�����ʱ����
		�ƶ���󣬽���ǰλ�ø�ֵΪ��ʱ����
*/
#include <iostream>
using namespace std;

//Ƶ������
void insertSortBad(int *a, int n){
	int i, j;
	for (i = 1; i < n; ++i){
		for (j = i; a[j - 1] > a[j] && j > 0; --j){
			swap(a[j - 1], a[j]);
		}
	}
}
//��Ƶ������ת��Ϊ��ֵ
void insertSort(int *a, int n){
	//��ʱ����,�����Ҫ�ƶ���Ԫ��
	int temp = 0;
	int i, j;
	for (i = 1; i < n; ++i){
			temp = a[i];
			for (j = i; a[j - 1] > temp && j > 0; --j){
				a[j] = a[j - 1];
			}
		a[j] = temp;
	}
}

int main(){
	int a[7] = { 2, 1, 3, 4, 5, 9, 8 };
	insertSortBad(a, 7);
	for (int i = 0; i < 7; ++i)
		cout << a[i] << endl;

	system("pause");
	return 0;
}