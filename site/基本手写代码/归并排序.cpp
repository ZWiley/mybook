//�������д���ͬλ�����ұ����Ƚϣ����ݴ�С�����������������е�ǰ���ǣ������������ж��Ǵ�С�������е�

//�鲢����˼�룺�ȵݹ��ٹ鲢
//�ȵݹ飬�����ҹ鲢Ϊ�������У��ٽ����ҽ��й鲢
//����������鲢ʱ���������д���ͬλ�����ұ����Ƚϣ����ݴ�С������������������
//���ĺ�����merge��Msort�����ǵݹ���ε���merge�Ĺ���

//�鲢����ƽ�����O(nlogn),���O(nlogn)���O(nlogn)���ռ临�Ӷ�O(n + logn)���ȶ�

//����ʱ�临�Ӷ�Ϊnlogn
//һ�˹鲢Ҫ��������������Ԫ�ر���һ�飬���Ӷ�ΪO(n)
//�ݹ����ĸ߶�Ϊlogn��ÿ�εݹ鶼Ҫһ�˹鲢
//nlogn

//���Ϳռ临�Ӷ�O(n + logn)
//��Ҫ�ǵݹ���ɵ�ջ�ռ��ʹ��logn����ÿ�ι鲢����Ҫ��ԭ����鲢�������飬�ٿ�����ԭ���飬��ÿ�ι鲢����Ҫ��ԭ����鲢�������飬�ٿ�����ԭ����O(n)
//O(n + logn)


#include <iostream>
#include <vector>
using namespace std;

//�����ǽ�sr����鲢�����ݣ��������н�tr��
//sr[s...m, m+1...t]
//int num = 0;
void merge(int sr[], int s, int m, int t){
	int length = t - s + 1;
	int *tr = new int[length];

	int i = s, j = m + 1, k = 0;

	while(i <= m && j <= t){
		if (sr[i] < sr[j])
			tr[k++] = sr[i++];
		else{
			//num += m - i + 1;
			tr[k++] = sr[j++];
		}
	}
	//���������ݳ��Ȳ�����ȫ���ʱ��ֻ��Ҫ�����Ҳ�����ʣ�����ݿ�������
	while (i <= m)
		tr[k++] = sr[i++];
	while (j <= t)
		tr[k++] = sr[j++];

	//�����������Ԫ�ؿ�����ԭ������
	for (k = 0; k < length; k++)
		sr[s + k] = tr[k];
}

//sr[s, t]ԭ����
void msort(int sr[], int s, int t){

	if (s >= t)
		return;

	//����ƽ���е�
	int m = s + (t - s) / 2;

	msort(sr, s, m);
	msort(sr, m+1, t);

	//�Ż�����Ϊ�������඼������õ����飬ֻ�е�����������Ҳ���Сʱ�źϲ�����Ϊ�ϲ���ζ�űȽϣ����˷�ʱ��
	if (sr[m] > sr[m+1])
		merge(sr, s, m, t);

}

int main(){

	int sr[10] = { 2, 1, 3, 78, 87, 53, 13, 20, 0, 10};
	
	msort(sr, 0, 9);

	//std::cout << num << endl;

	for (int i = 0; i < 10; i++){
		std::cout << sr[i] << endl;
	}
	std::system("pause");
	return 0;
}
