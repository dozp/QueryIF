#include <iostream>
#include <vector>
#include <stack>
#include <memory>
#include <fstream>
#include <string>
#include <cctype>
#include <thread>
#include <mutex>
//#include <regex>

using namespace std;

enum class status
{
	open,
	brace_open,
	brace_close,
	without_brace,
	close
};

mutex os_mutex;

class IfInfo {
//friend ostream& print(ostream&, const IfInfo&);	
public:
	IfInfo()=default;
	IfInfo(int line, int column):line(line),column(column),_status(status::open){
		height=0;	
	};
	int line;
	int column;
	int height;
	vector<shared_ptr<IfInfo>> children;
	void push_brace(){
		_status=status::brace_open;
		s_brace.push('{');
	};

	void pop_brace(){
		if(!s_brace.empty())
		{
			s_brace.pop();
			if(s_brace.empty())
				_status=status::brace_close;
		}
	};

	void close()
	{
		_status=status::close;
	};

	status get_status(){
		return _status;
	}

	void set_status(status val){
		_status=val;
	}
private:
	/*void ToList(vector<shared_ptr<IfInfo>> &ls)
	{
		ls.push_back(this);
		for(int i=0; i<this.children.size();++i)
		{
			this.children[i].ToList(ls);
		}
		return cnt;
	}*/
	stack<char> s_brace;
	status _status;

};

ostream& print(ostream &os, vector<shared_ptr<IfInfo>> &vIf, const char* fileName)
{
	lock_guard<mutex> guard(os_mutex);
	os<<fileName<<":";
	os << "Totally:" <<vIf.size()<<" ";
	for(auto i : vIf)
		os << "(" << i->line << ","<< i->column<<") ";
	os<<endl;
	int nestedCnt=0;
	for(auto i : vIf)
	{
		if(i->height>0)
			nestedCnt++;
	}
	os<<"Total nested if:"<<nestedCnt<<endl;
	for(auto i : vIf)
	{
		if(i->height>0)
			os << "height:"<<i->height<<" (" << i->line << ","<< i->column<<") "<<endl;
	}
	os<<endl;

	return os;
}

void pop_IfInfo_stack(stack<shared_ptr<IfInfo>> &s_IfInfo){
	if(s_IfInfo.empty()) return;
	auto p=s_IfInfo.top();
	decltype(p) p0=NULL;
	s_IfInfo.pop();
	if(!s_IfInfo.empty())
		p0=s_IfInfo.top();
	if(p0!=NULL)
	{
		if(p0->children.size()>0 && p0->children[0]==p && p0->get_status()==status::without_brace)
		{
			s_IfInfo.pop();
			pop_IfInfo_stack(s_IfInfo);
		}
	}
}

bool validQuote(const char *p, const char *head)
{
	if(p==head)
		return true;
	if(p>head && *(p-1)!='\\')
	{
		return true;
	}
	else if(string(p-2,2)=="\\\\")
		return true;
	return false;
}

void ParseLine(vector<shared_ptr<IfInfo>> &vIf, stack<shared_ptr<IfInfo>> &s_IfInfo, const string &text, bool &bBlockComment, bool &bQuoted, char &quoteChar, bool &bElse, int lineNo)
{
	const char *p = text.c_str(), *p1=p;
	bool bNewIf=false;
	bool bCommentLine=false;
	bool bEnd=false;
	bool bBraceStart=false;
	bool bBraceEnd=false;
	while(bBlockComment)
	{
		if(string(p,2)=="*/")
		{
			p+=2;
			bBlockComment=false;
		}
		else if(*p!='\0')
			++p;
		else
			break;
	}
	
	while(!bBlockComment && !bCommentLine )
	{
		if(*p=='\0')
		{
			break;
		}
		
		if(*p=='"'||*p=='\'')
		{
			if(validQuote(p, p1))
				if(!bQuoted)
				{
					bQuoted=true;
					quoteChar=*p;
				}else if(*p==quoteChar){
					bQuoted=false;
				}
		}
		else if(bQuoted)
		{
			++p;
			continue;
		}
		else if(*p==' ' || *p=='\t')
		{
			++p;
			continue;
		}
		else if(*p=='{')
		{
			bBraceStart=true;
		}
		else if(*p=='}')
		{
			bBraceEnd=true;
		}
		else if(*p==';')
		{
			bEnd=true;
		}
		else if(string(p,2)=="if")
		{
			if(*(p+2)==' ' || *(p+2)=='\t' || *(p+2)=='(' || *(p+2)=='\0')
			{
				if(p==p1||*(p-1)==' ' || *(p-1)=='\t' || *(p-1)==';')
				{
					bNewIf=true;
				}
			}
		}
		else if(string(p,2)=="//")
		{
			bCommentLine=true;
			continue;
		}
		else if(string(p,2)=="/*")
		{
			bBlockComment=true;
			while(bBlockComment)
			{
				if(string(p,2)=="*/")
				{
					p++;
					bBlockComment=false;
				}
				else if(*p!='\0')
					++p;
				else
					break;
			}
		}
		else if(!s_IfInfo.empty() && s_IfInfo.top()->get_status()==status::without_brace )
		{
			if( string(p,4)=="else" && 
			    (*(p+4)==' ' || *(p+4)=='\t' || *(p+4)=='\0') &&
			    (*(p-1)==' ' || *(p-1)=='\t' || p==p1 || *(p-1)=='}')
			  )
			{
				bElse=true;
			}
			/*else
			{
				s_IfInfo.top()->close();
				s_IfInfo.pop();
			}*/
		}


		if( bEnd || bBraceStart || bBraceEnd)
		{
			bElse=false;
		}

		if(!s_IfInfo.empty() && s_IfInfo.top()->get_status()==status::without_brace )
		{
			if((bNewIf && !bElse) || bBraceStart || bBraceEnd || bEnd)
			{
				pop_IfInfo_stack(s_IfInfo);
			}	
		}
		
		if(bNewIf)
		{
			bNewIf=false;
			auto pIfInfo = make_shared<IfInfo>(lineNo, p-p1+1);
			vIf.push_back(pIfInfo );
			if(!s_IfInfo.empty())
			{
				s_IfInfo.top()->children.push_back(pIfInfo);
				if(s_IfInfo.top()->get_status()==status::open)
				{
					s_IfInfo.top()->set_status(status::without_brace);
				}
				pIfInfo->height=s_IfInfo.top()->height+1;
				s_IfInfo.push(pIfInfo);
			}
			else
			{
				s_IfInfo.push(pIfInfo);
			}
		}
		else if(bEnd)
		{
			bEnd=false;
			if(!s_IfInfo.empty())
			{
				auto p=s_IfInfo.top();
				if(p->get_status()==status::open)
				{
					p->close();
					s_IfInfo.pop();
				}
			}
		}
		else if(bBraceStart)
		{
			bBraceStart=false;
			if(!s_IfInfo.empty())
			{
				auto p=s_IfInfo.top();
				p->push_brace();
			}
		}
		else if(bBraceEnd)
		{
			bBraceEnd=false;
			if(!s_IfInfo.empty())
			{
				auto p=s_IfInfo.top();
				if(p->get_status()==status::brace_open)
				{
					p->pop_brace();
					if(p->get_status()==status::brace_close)
					{
						p->close();
						s_IfInfo.pop();
					}
				}
			}
		}
		++p;	
	}
	if(*p=='\0' || bCommentLine)
		return;
};

void ParseFile(const char* fileName, ostream& os){
	ifstream input(fileName);
	string text;
	stack<shared_ptr<IfInfo>> s_IfInfo;
	vector<shared_ptr<IfInfo>> vIf;
	auto pIfInfo = make_shared<IfInfo>();
	bool bBlockComment=false;
	bool bQuoted=false;
	bool bElse=false;
	char quoteChar='\0';
	int lineNo=1;
	while(getline(input, text)){
		ParseLine(vIf, s_IfInfo, text, bBlockComment,bQuoted, quoteChar, bElse, lineNo++);
	}
	input.close();
	print(os, vIf, fileName);
};

void hello()
{
	cout<<"hello"<<endl;
}
int main(int argc, char* argv[])
{
	//char default_file[]="./Test.java";
	char* fileName[50];
	/*if(argc>1)
		fileName=argv[1];
	else
		fileName=default_file;*/
	int InputFileCnt=argc-1;
	ofstream output("./result.txt");
	for(int i=0; i<InputFileCnt; ++i)
	{
		fileName[i]=argv[i+1];
		//ParseFile( fileName[i],output);
		thread t(ParseFile, fileName[i],ref(output));
		//thread t(hello);
		t.join();
	}
	//ParseFile(fileName, cout);
}


