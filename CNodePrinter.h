#ifndef __CNODEPRINTER__
#define __CNODEPRINTER__
#include<wx/string.h>
#include"logocore.h"

typedef char uchar;

class CNodePrinter
{
public:
	CNodePrinter();
	~CNodePrinter();
public:
	virtual wxString& Print(NODE* n);

	virtual wxString& GetContent() { return this->content; }

	virtual bool& GetBackslashSetting() { return this->bs; }

protected:

	virtual void PrintChar(uchar ch);
	virtual void PrintSpace();

	virtual void NdPrintf(const uchar* fmt, ...);

	virtual void NewLine();

	virtual void PrintList(const NODE *ndlist);

	virtual void PrintNode(const NODE * nd);

protected:
	wxString content;

	bool bs;

};
#endif

