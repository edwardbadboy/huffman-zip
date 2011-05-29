//notice:没有考虑字节序的问题

#include <iostream>//基本流操作
#include <fstream>//文件
#include <vector>//需要使用向量
#include <queue>
#include <algorithm>//需要使用标准库的几个算法
#include <limits>//需要使用long最大值
#include <cstdlib>//需要使用system函数
#include "Bitstream.imp.h"//使用了开源的Bitstream库

//每个压缩文件的文件头都设置为下面的字符串，用以识别文件是否是本程序压缩过的文件
#define MAGIC_VERSION "huffman zipped file version 1"

//using namespace std;
using std::vector;
using std::string;
using std::priority_queue;
using std::binary_function;
using std::istream;
using std::ostream;
using std::ifstream;
using std::ofstream;
using std::cout;
using std::cin;
using std::clog;
using std::ios;
using std::ios_base;
using std::endl;
using std::dec;
using std::hex;
using std::numeric_limits;
using namespace Costella;//使用了开源的Bitstream库，这个库的内容全在此名称空间里

//huffman树的一个节点
struct HuffmanNode{
	long lchild;//左孩子下标
	long rchild;//右孩子下标
	long parent;//双亲下标
	long weight;//权重
};

//词汇表的一个项目
struct HuffmanToken{
	unsigned char byte;//单词内容
	long weight;//权重
};

//编码表的一个项目
struct HuffmanCode{
	unsigned char byte;//单词
	vector<int> code;//单词对应编码，由一连串的0、1序列组成
};

typedef vector<HuffmanNode> HuffmanTree;//huffman树
typedef vector<HuffmanToken> TokenList;//词汇表
typedef vector<HuffmanCode> HuffmanCodes;//编码表

struct HuffmanNodeComparer //: public std::binary_function<HuffmanNode*, HuffmanNode*, bool>
{
	bool operator()(const HuffmanNode *l, const HuffmanNode *r) const{
		return l->weight > r->weight;
	}
};

typedef priority_queue<HuffmanNode*, vector<HuffmanNode*>, HuffmanNodeComparer> HuffmanQueue;//用于提取最小根元素的优先队列

//检测词汇表项目的权重是否是0
bool is_empty_token(const HuffmanToken &tk){
	return tk.weight==0;
}

/*
bool compare_HuffmanNode(const HuffmanNode &node1,const HuffmanNode &node2){
	return (node1.lchild==node2.lchild) && (node1.rchild==node2.rchild) && (node1.parent==node2.parent) && (node1.weight==node2.weight);
}

bool compare_HuffmanToken(const HuffmanToken &ht1,const HuffmanToken &ht2){
	return (ht1.byte==ht2.byte) && (ht1.weight==ht2.weight);
}*/

//遍历输入文件，建立词汇表
TokenList collect_word_list(istream &in){
	TokenList tokens;
	TokenList result_tokens;
	unsigned char byte;
	//每个字节作为一个单词，由于1字节内可以有0-255共256种可能的值，因此单词一共有256个
	//在词汇表中插入256个单词，令每个单词的权为0，作为初始值
	for(int i=0;i<256;++i){//i的类型不能是char，否则会在256的时候回绕到0
		HuffmanToken token={i,0};
		tokens.push_back(token);
	}
	//遍历输入文件，统计每个单词出现的次数
	while(in){
		if(!in.read(reinterpret_cast<char*>(&byte),sizeof(byte))){
			break;
		}
		++(tokens[byte].weight);//统计每个单词出现的次数
	}
	//将没出现过的单词过滤掉，只出现过的单词到我们的结果中
	remove_copy_if(tokens.begin(),tokens.end(),back_inserter(result_tokens),is_empty_token);
	//remove_copy_if是标准库的
	return result_tokens;
}

//用词汇表的权重初始化huffman树（其实是森林，最后才合并成树）
void init_huffman_tree(HuffmanTree &ht,const TokenList &tokens){
	HuffmanNode default_node={-1,-1,-1,0};//初始每个节点的左右孩子和父亲下标都是-1，权重是0
	TokenList::size_type n;
	ht.clear();
	n=tokens.size();
	ht.assign(2*n-1,default_node);//n个词汇的huffman树需要2*n-1个节点，把这2*n-1个节点都设置成默认值
	for(TokenList::size_type i=0;i<n;++i){
		ht[i].weight=tokens[i].weight;//用词汇表中的权重初始化huffman树的前n个节点
	}
	return;
}

void init_huffman_queue(HuffmanQueue &q, const HuffmanTree &ht, HuffmanTree::size_type size){
	for(HuffmanTree::size_type i=0; i < size; ++i){
		q.push( const_cast<HuffmanNode*>(&ht[i]) );
	}
	return;
}

//寻找huffman树（其实是森林，最后才合并成树）中权重最小和次小的根节点
//使用优先队列
void find_min_weight_positions(const HuffmanTree &ht, HuffmanQueue &q,long &min_pos1,long &min_pos2){
	HuffmanNode *n1=NULL, *n2=NULL;
	n1=q.top();
	min_pos1= n1 - &ht[0];
	q.pop();
	n2=q.top();
	min_pos2= n2 - &ht[0];
	q.pop();
	return;
}

//创建huffman树，找最小元素使用优先队列
void create_huffman_tree(HuffmanTree &ht, const TokenList &tokens){
	init_huffman_tree(ht, tokens);//用词汇表的全部n个项目的权重初始化树（其实是森林）
	HuffmanTree::size_type nodecount=ht.size();
	TokenList::size_type wordcount=tokens.size();

	HuffmanQueue q;
	init_huffman_queue(q, ht, wordcount);

	//现在0到n-1个节点的权重是词汇表中的权重值
	//从n到2*n-2做共n-1次合并，每次找到最小和次小的根的权重，合并根和权重，合并后的节点存储下来
	for(TokenList::size_type i=wordcount; i<nodecount; ++i){
		long min_pos1=-1,min_pos2=-1;//最小值和次小值
		find_min_weight_positions(ht,q,min_pos1,min_pos2);//找到权重最小和次小的两个根
		ht[min_pos1].parent=ht[min_pos2].parent=i;//这两个根变为当前节点的子节点
		ht[i].lchild=min_pos1;//一个作为当前的左孩子
		ht[i].rchild=min_pos2;//另一个作为当前节点的右孩子
		ht[i].weight=ht[min_pos1].weight+ht[min_pos2].weight;//当前节点的权重为两个子节点权重的和
		q.push(&ht[i]);
	}
	//循环结束后ht[hi.size()-1]中的节点就是huffman树的根，0到n-1是叶子节点，剩下的是中间节点
	return;
}

//find_min_weight_positions的非优先队列版
//void find_min_weight_positions(const HuffmanTree &ht,long end,long &min_pos1,long &min_pos2){
	//long min_weight1,min_weight2;//最小的和次小的节点权重
	//min_weight1=min_weight2=numeric_limits<long>::max();//初始化权重为long型的最大值
	//long i;
	//for(i=0;i<end;++i){
		//if(ht[i].parent!=-1){//parent=-1的才是根节点，其他的忽略
			//continue;
		//}
		//if(ht[i].weight<=min_weight1){//这里判断关系时，必须写成小于等于，如果只写成小于，那么存在两个同weight而pos不同的元素时，就会漏掉其中一个
			//min_weight2=min_weight1;//如果当前节点比已找到的最小值还小，那么最小值就要变成当前节点，原来的最小值变为次小值
			//min_pos2=min_pos1;
			//min_weight1=ht[i].weight;
			//min_pos1=i;
		//}else if(ht[i].weight<min_weight2){//如果当前节点比次小值小，那么就记录新的次小值
			//min_weight2=ht[i].weight;
			//min_pos2=i;
		//}
	//}
	//return;
//}

//创建huffman树的非优先队列版
//void create_huffman_tree(HuffmanTree &ht,const TokenList &tokens){
	//init_huffman_tree(ht,tokens);//用词汇表的全部n个项目的权重初始化树（其实是森林）
	//HuffmanTree::size_type nodecount=ht.size();

	////现在0到n-1个节点的权重是词汇表中的权重值
	////从n到2*n-2做共n-1次合并，每次找到最小和次小的根的权重，合并根和权重，合并后的节点存储下来
	//for(TokenList::size_type i=wtokens.size(); i<nodecount; ++i){
		//long min_pos1=-1,min_pos2=-1;//最小值和次小值
		//find_min_weight_positions(ht,i,min_pos1,min_pos2);//找到权重最小和次小的两个根
		//ht[min_pos1].parent=ht[min_pos2].parent=i;//这两个根变为当前节点的子节点
		//ht[i].lchild=min_pos1;//一个作为当前的左孩子
		//ht[i].rchild=min_pos2;//另一个作为当前节点的右孩子
		//ht[i].weight=ht[min_pos1].weight+ht[min_pos2].weight;//当前节点的权重为两个子节点权重的和
	//}
	////循环结束后ht[hi.size()-1]中的节点就是huffman树的根，0到n-1是叶子节点，剩下的是中间节点
	//return;
//}

//通过huffman树，创建某个单词的huffman编码
void create_huffman_code(const HuffmanTree &ht,long ht_index,unsigned char byte,HuffmanCode &hc){
	long i=ht_index;//我们要创建编码的单词在huffman树中的下标
	hc.byte=byte;//记录我们要创建编码的单词

	//从huffman树的叶子节点开始往父节点走
	//如果当前节点是父节点的左孩子，那么编码为0，是右孩子，编码为1
	//就这样走到根节点后，求得了一连串编码，然后把它反转过来，就是需要的huffman编码
	while(ht[i].parent!=-1){//根节点的parent都是-1，如果还没走到根节点，就继续走
		if(ht[ht[i].parent].lchild==i){//如果当前节点是父节点的左孩子
			hc.code.push_back(0);//新增加编码0
		}else{//如果当前节点是父节点的右孩子
			hc.code.push_back(1);//新增加编码1
		}
		i=ht[i].parent;//往根节点走一步
	}
	reverse(hc.code.begin(),hc.code.end());//反转刚才得到的编码，才是我们要求的结果
	//reverse是标准库的算法
	return;
}

//通过huffman树，为所有单词创建编码
void create_huffman_codes(const HuffmanTree &ht, const TokenList &tokens, HuffmanCodes &hcs){
	hcs.clear();
	TokenList::size_type wordcount=tokens.size();
	//huffman编码集合初始化
	for(int i=0;i<256;++i){//总共有256种可能的单词
		HuffmanCode hc;hc.byte=i;
		hcs.push_back(hc);
	}

	//对每个在词汇表中的单词创建编码，不在词汇表中的单词就不管了
	for(TokenList::size_type i=0; i<wordcount; ++i){
		HuffmanCode hc;
		unsigned char byte=tokens[i].byte;
		create_huffman_code(ht,i,byte,hc);//为词汇表的第i项创建编码
		hcs[byte].code=hc.code;//将创建好的编码存放到huffman编码集合中覆盖原来的默认值
	}

	//循环结束后，huffman编码集合hcs中包含256个元素，其中在词汇表中出现过的单词已经创建了编码
	//没有在词汇表中出现的单词就没有创建
	//要求假设字节（单词）的值为100，那么其对应编码就是hcs[100].code
}

//将我们创建的huffman编码集合的某一项的编码打印出来
void print_huffman_code(const HuffmanCode &hc,const TokenList &tk,long &tk_i){
	if(hc.code.size()==0){//没在词汇表中的单词是没有编码的，不打印，直接返回
		return;
	}
	if(isgraph(hc.byte)){
		cout<<"'"<<hc.byte<<"'";
	}else{
		cout<<"0x"<<hex<< static_cast<int>(hc.byte)<<dec;
	}
	cout<<"\tweight "<<tk[tk_i].weight<<"\tcode:";
	vector<int>::const_iterator iter, iter_end;
	for(iter=hc.code.begin(), iter_end=hc.code.end(); iter!=iter_end; ++iter){
		cout<<" "<<(*iter);
	}
	++tk_i;
	cout<<endl;
}

//打印我们创建的huffman编码
void print_huffman_codes(const HuffmanCodes &hcs,const TokenList &tk){
	long tk_i=0;
	HuffmanCodes::const_iterator iter, iter_end;
	for(iter=hcs.begin(), iter_end=hcs.end(); iter!=iter_end; ++iter){//遍历编码集合
		print_huffman_code((*iter),tk,tk_i);//打印每一项
	}
}

//将huffman树的某一节点输出到文件中
bool write_huffman_node(ostream &out,const HuffmanNode &node){
	out.write(reinterpret_cast<const char*>(&(node.lchild)),sizeof(node.lchild));//没有考虑字节序
	out.write(reinterpret_cast<const char*>(&(node.rchild)),sizeof(node.rchild));
	out.write(reinterpret_cast<const char*>(&(node.parent)),sizeof(node.parent));
	//out.write(reinterpret_cast<const char*>(&(node.weight)),sizeof(node.weight));//不用输出权重，权重只在求huffman树的时候有用
	if(out){
		return true;
	}else{
		return false;
	}
}

//将词汇表的某一项输出到文件中
bool write_huffman_token(ostream &out,const HuffmanToken &tk){
	out.write(reinterpret_cast<const char*>(&(tk.byte)),sizeof(tk.byte));
	out.write(reinterpret_cast<const char*>(&(tk.weight)),sizeof(tk.weight));
	if(out){
		return true;
	}else{
		return false;
	}
}

//将huffman树和词汇表输出到文件中
bool write_huffman_tree(ostream &out,const HuffmanTree &ht,const TokenList &tokens)
{
	long n=tokens.size();
	out.write(reinterpret_cast<const char*>(&n),sizeof(n));//输出词汇表的项目数，不考虑字节序
	if(!out){
		return false;
	}

	HuffmanTree::const_iterator huffiter, huffiter_end;//输出huffman树的每个节点
	for(huffiter=ht.begin(), huffiter_end=ht.end(); huffiter!=huffiter_end; ++huffiter){
		if(write_huffman_node(out,(*huffiter))==false){
			return false;
		}
	}

	TokenList::const_iterator tokeniter, tokeniter_end;//输出词汇表的每个节点
	for(tokeniter=tokens.begin(), tokeniter_end=tokens.end(); tokeniter!=tokeniter_end; ++tokeniter){
		if(write_huffman_token(out,(*tokeniter))==false){
			return false;
		}
	}
	return true;
}

//从文件中读取一个huffman树的一个节点
bool read_huffman_node(istream &in,HuffmanNode &node){
	in.read(reinterpret_cast<char*>(&(node.lchild)),sizeof(node.lchild));
	in.read(reinterpret_cast<char*>(&(node.rchild)),sizeof(node.rchild));
	in.read(reinterpret_cast<char*>(&(node.parent)),sizeof(node.parent));
	//in.read(reinterpret_cast<char*>(&(node.weight)),sizeof(node.weight));//不用读权重，因为我们没有把权重写到文件里去
	if(in){
		return true;
	}else{
		return false;
	}
}

//从文件中读取词汇表的一项
bool read_huffman_token(istream &in,HuffmanToken &tk){
	in.read(reinterpret_cast<char*>(&(tk.byte)),sizeof(tk.byte));
	in.read(reinterpret_cast<char*>(&(tk.weight)),sizeof(tk.weight));
	if(in){
		return true;
	}else{
		return false;
	}
}

//从文件中读取huffman树和词汇表
bool read_huffman_tree(istream &in,HuffmanTree &ht,TokenList &tokens)
{
	long n=0,i=0,ht_n=0;;
	in.read(reinterpret_cast<char*>(&n),sizeof(n));//读取词汇表的长度
	if(!in){
		return false;
	}

	ht.clear();
	ht_n=2*n-1;
	for(i=0;i<ht_n;++i){//读取2*n-1个huffman树的节点
		HuffmanNode node;
		if(read_huffman_node(in,node)==false){
			return false;
		}
		ht.push_back(node);
	}

	tokens.clear();
	for(i=0;i<n;++i){//读取全部的词汇表
		HuffmanToken tk;
		if(read_huffman_token(in,tk)==false){
			return false;
		}
		tokens.push_back(tk);
	}
	return true;
}

//将标志头写到压缩文件里去
bool write_huffman_zip_header(ostream &out){
	out<<MAGIC_VERSION<<"\n";
	if(out){
		return true;
	}else{
		return false;
	}
}

//从压缩文件里读取第一行作为标志头
string read_huffman_zip_header(istream &in){
	string header;
	getline(in,header);
	return header;
}

//将编码过的内容写到文件里去
void write_huffman_code(Bitstream::Out<long> &bout, const vector<int> &code){
	vector<int>::const_iterator iter, iter_end;
	for(iter=code.begin(), iter_end=code.end(); iter!=iter_end; ++iter){
		bout.boolean((*iter)==1);//把编码的每个项目写到比特流里
	}
	//关于bout.boolean：如果bout.boolean(true)，会写比特'1'到文件里，如果是bout.boolean(false)，就写'0'到文件里。
	return;
}

//通过为输入文件创建huffman的编码，并把编码后的内容写到输出文件
bool huffman_data_encode(istream &in,ostream &out,const HuffmanCodes &hcs)
{
	long write_start_pos=out.tellp();//我们需要记录一开始写输出文件的位置，等下要跳回来
	long bit_count=0;//记录一共写了多少个比特
	//要在文件中记录一共写了多少个比特，但是这个值要写完整个文件才知道
	//所以我们要为这个值在文件中预留一个位置，写完整个文件后再跳回来把实际写了多少个比特写到文件中
	out.write(reinterpret_cast<const char*>(&bit_count),sizeof(bit_count));
	if(!out){
		return false;
	}
	
	Bitstream::Out<long> bout(out);//创建比特流输出对象
	bit_count=bout.position();
	while(in){
		unsigned char byte=0;
		if(!in.read(reinterpret_cast<char*>(&byte),sizeof(byte))){//从输入文件中读一个单词
			break;
		}
		write_huffman_code(bout,hcs[byte].code);//把此单词对应的编码写到输出文件中
		if(!out){
			return false;
		}
	}

	bit_count=bout.position()-bit_count;//看看我们写了多少个比特
	bout.flush();//把缓存中的数据全部实际的写到文件中
	out.seekp(write_start_pos,ios::beg);//跳回文件头部
	out.write(reinterpret_cast<const char*>(&bit_count),sizeof(bit_count));//记录一下我们写了多少比特
	out.seekp(0,ios::end);//跳到文件尾部，这个操作其实可以不做，因为等下我们就要关闭文件了，不再写了
	if(out){
		return true;
	}else{
		return false;
	}
}

void print_tokens(const TokenList &tokens){
	TokenList::size_type wordcount=tokens.size();
	for(TokenList::size_type i=0; i<wordcount; ++i){
		cout<<i<<" token "<<tokens[i].byte<<" weight "<<tokens[i].weight<<"\r\n";
	}
	return;
}

//使用huffman树的原理进行压缩的函数
bool huffman_zip(const char *in_filename,const char *out_filename)
{
	HuffmanTree ht;//huffman树
	TokenList tokens;//词汇表
	HuffmanCodes hcs;//huffman编码集合
	ifstream in;//输入文件
	ofstream out;//输出文件
	bool r=false;//操作是否成功，不成功就是false

	//必须用binary方式打开输入文件，否则系统会在遇到0x0d连着0x0a的时候，把0x0d吞掉
	//0x0d是'\r'，0x0a是'\n'，如果不使用binary标志，就默认用文本模式打开流，因此会出现
	//这种转换
	in.open(in_filename,ios_base::in|ios_base::binary);
	if(!in){
		clog<<"无法打开输入文件："<<in_filename<<endl;
		return false;
	}
	tokens=collect_word_list(in);//扫描输入文件，得到词汇表
	if(tokens.size()==0){
		clog<<"文件为空："<<in_filename<<endl;
		return false;
	}
	//print_tokens(tokens);
	//文件已经读到头了，现在是无效状态，我们要把它倒回头，等下好开始读里面的内容好用来做huffman压缩
	in.clear();
	in.seekg(0,ios::beg);
	if(!in){
		clog<<"无法移动输入文件指针："<<in_filename<<endl;
		return false;
	}

	create_huffman_tree(ht,tokens);//从词汇表和权重创建huffman树
	create_huffman_codes(ht,tokens,hcs);//从huffman树、词汇表创建huffman编码集合
	/*cout<<"下面是生成的huffman编码表："<<endl;//输出我们创建的编码表看看
	print_huffman_codes(hcs,tokens);*/

	out.open(out_filename,ios_base::out|ios_base::binary);//打开输出文件，此处一定要用binary模式
	if(!out){
		clog<<"无法打开输出文件："<<out_filename<<endl;
		return false;
	}
	if(write_huffman_zip_header(out)==false){//写标志头
		clog<<"无法写输出文件："<<out_filename<<endl;
		return false;
	}
	if(write_huffman_tree(out,ht,tokens)==false){//写huffman树和词汇表
		clog<<"无法写输出文件："<<out_filename<<endl;
		return false;
	}
	if(huffman_data_encode(in,out,hcs)==false){//把in里的内容编码后输出到out
		clog<<"无法写输出文件："<<out_filename<<endl;
		return false;
	}
	in.close();
	out.close();
	return true;
}

//通过从输入文件中读出的huffman树，对输入解码并把结果写到输出文件
bool huffman_data_decode(istream &in,ostream &out,const HuffmanTree &ht,const TokenList &tokens)
{
	long bit_count=0;//从文件中读出原来写下的比特数，放在这里
	long read_count=0;//记录我们在后面操作中实际读出的比特数

	in.read(reinterpret_cast<char*>(&bit_count),sizeof(bit_count));//读出此文件后面内容占的比特数
	if(!in){
		return false;
	}
	if(tokens.size()==1){//如果词汇表只有一项，那就简单了，直接把这一项输出weight次到out中就行
		for(long i=0;i<tokens[0].weight;++i){
			out.write(reinterpret_cast<const char*>(&(tokens[0].byte)),sizeof(tokens[0].byte));
			if(!out){
				return false;
			}
		}
		return true;
	}

	Bitstream::In<long> bin(in);//比特流的输入
	//解码的过程是，从huffman树的根开始往叶子走，在读到比特0时往左，在读到1的时候往右
	//走到叶子的时候，就是解压缩的结果
	long rootpos=ht.size()-1;
	long huffpos=rootpos;//当前解码的位置
	while(in && read_count<bit_count){//如果我们还没解码完所有的比特
		bool bit=false;//读出的比特是0还是1，如果是0，bit就是false，如果是1，bit就是true
		bin.boolean(bit);//从文件中读出一个比特存到变量bit里
		if(!in){break;}//文件读完了
		if(bit){
			if(ht[huffpos].rchild==-1){
				return false;//没路可走了，说明解码出错，输入文件可能被损坏了
			}else{
				huffpos=ht[huffpos].rchild;//在读到1的时候往右走
			}
		}else{
			if(ht[huffpos].lchild==-1){
				return false;//没路可走了，说明解码出错，输入文件可能被损坏了
			}else{
				huffpos=ht[huffpos].lchild;//在读到比特0时往左走
			}
		}
		//看看是不是已经走到叶子了
		if(ht[huffpos].lchild==-1 && ht[huffpos].rchild==-1){//叶子节点的lchild和rchild肯定都是-1
			//把叶子节点对应的单词输出到out
			out.write(reinterpret_cast<const char*>(&(tokens[huffpos].byte)),sizeof(tokens[huffpos].byte));
			if(!out){
				return false;
			}
			huffpos=rootpos;//把当前位置移动到根节点，开始下一轮解码
		}
		++read_count;//积累我们已经处理过的比特数
	}
	return true;
}

//使用huffman树的原理进行解压缩的函数
bool huffman_unzip(const char *in_filename,const char *out_filename)
{
	HuffmanTree ht;//huffman树
	TokenList tokens;//词汇表
	ifstream in;//输入文件（压缩过的文件）
	ofstream out;//输出文件（解压缩后的文件）
	string header;//压缩过的文件头部的标志
	bool r=false;//操作成功为true，操作失败为false

	in.open(in_filename,ios_base::in|ios_base::binary);//必须用binary模式打开，否则系统会作多余的转换
	if(!in){
		clog<<"无法打开输入文件："<<in_filename<<endl;
		return false;
	}
	header=read_huffman_zip_header(in);//读压缩文件头
	if(header!=MAGIC_VERSION){//判断是否是我们压缩过的文件
		clog<<"无法读取输入文件，或着它不是hzip格式的压缩文件："<<in_filename<<endl;
		return false;
	}
	if(read_huffman_tree(in,ht,tokens)==false){//从文件中读出huffman树和词汇表，重建起这两个数据结构
		clog<<"无法从输入文件中读取元信息："<<in_filename<<endl;
	}

	out.open(out_filename,ios_base::out|ios_base::binary);//必须用binary模式打开，否则系统会作多余的转换
	if(!out){
		clog<<"无法打开输出文件："<<out_filename<<endl;
		return false;
	}
	r=huffman_data_decode(in,out,ht,tokens);//对输入文件解码
	if(r==false){
		clog<<"输入文件已损坏或写输出文件失败："<<endl<<"\t"<<in_filename<<endl<<"\t"<<out_filename<<endl;
		return false;
	}
	in.close();
	out.close();
	return r;
}

int main(int argc, char* argv[])
{
	string in_filename,zip_filename,out_filename;
	cout<<"请输入文件名或完整路径（回车表示输入完毕，不支持中文）："<<endl;
	getline(cin,in_filename);
	zip_filename=in_filename+".hzip";
	cout<<"正在压缩……"<<endl;
	if(huffman_zip(in_filename.c_str(),zip_filename.c_str())){
		cout<<"压缩已完成，输出文件："<<zip_filename<<endl;
	}else{
		cout<<"压缩失败，请查看历史记录以确定错误信息。"<<endl;
		system("pause");
		return 0;
	}
	out_filename=in_filename+".unhzip";
	cout<<"正在解压……"<<endl;
	if(huffman_unzip(zip_filename.c_str(),out_filename.c_str())){
		cout<<"解压已完成，输出文件："<<out_filename<<endl;
	}else{
		cout<<"解压失败，请查看历史记录以确定错误信息。"<<endl;
	}

	system("pause");
	return 0;
}

