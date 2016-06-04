#ifndef __XML_H__
#define __XML_H__

#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>

class CXMLNode : public std::vector<CXMLNode*>
{
private:
	char* m_pValue;
	char* m_pName;

	CXMLNode* m_pParentNode;

public:
	CXMLNode() :
		m_pParentNode(NULL),
		m_pValue(NULL),
		m_pName(NULL)
	{
	}

	CXMLNode(char* name) :
		m_pParentNode(NULL),
		m_pValue(NULL),
		m_pName(name)
	{
	}

	CXMLNode(char* name, char* value) :
		m_pParentNode(NULL),
		m_pValue(value),
		m_pName(name)
	{
	}

	~CXMLNode()
	{
		if (m_pName)
		{
			delete[] m_pName;
		}

		if (m_pValue)
		{
			delete[] m_pValue;
		}

		for (CXMLNode::iterator it = begin(); it != end(); ++it)
		{
			delete *it;
		}
	}

	inline CXMLNode* GetNode(const char* name)
	{
		for (CXMLNode::iterator it = begin(); it != end(); ++it)
		{
			if (strcmp((*it)->GetName(), name) == 0)
			{
				return *it;
			}
		}

		return NULL;
	}

	inline CXMLNode* GetParentNode()
	{
		return m_pParentNode;
	}

	inline const char* GetName()
	{
		return m_pName;
	}

	template<typename T> inline T GetValue()
	{
		T t;

		try
		{
			t = boost::lexical_cast<T>(m_pValue);
		}
		catch (...)
		{
			return T();
		}

		return t;
	}

	inline void SetParentNode(CXMLNode* pParentNode)
	{
		m_pParentNode = pParentNode;
	}

	inline void PushNode(CXMLNode* pNode)
	{
		push_back(pNode);
	}

	inline void SetValue(char* pValue)
	{
		m_pValue = pValue;
	}
};

#define MEMCHR(x,y,z) (char*)(memchr(x,y,z))

class CXMLDocument
{
private:
	CXMLNode* m_pMainNode;

public:
	inline CXMLNode* Node()
	{
		return m_pMainNode;
	}

public:
	CXMLDocument() :
		m_pMainNode(NULL)
	{
	}

	~CXMLDocument()
	{
		if (m_pMainNode)
		{
			delete m_pMainNode;
		}
	}

	bool ReadXML(std::istream& stream)
	{
		ReadXML(static_cast<std::stringstream&>(std::stringstream() << stream.rdbuf()).str().c_str());
	}

	bool ReadXML(const char* pData, size_t iSize = 0)
	{
		if (!pData)
		{
			return false;
		}

		if (!iSize)
		{
			iSize = strlen(pData);
		}

		char* pCurrent = MEMCHR(pData, '\n', iSize) + 1;
		char* pEnd = (char*)pData + iSize;

		if (!pCurrent)
		{
			return false;
		}

		CXMLNode* pParentNode = NULL;

		while (pCurrent)
		{
			char* pBegin = MEMCHR(pCurrent, '<', pEnd - pCurrent);

			if (!pBegin)
			{
				break;
			}

			if (pBegin[1] == '/')
			{
				if (pParentNode)
				{
					pParentNode = pParentNode->GetParentNode();
				}

				pCurrent = MEMCHR(pCurrent + 1, '\n', pEnd - pBegin) + 1;

				if (pCurrent - 1 == NULL)
				{
					break;
				}

				continue;
			}

			char* pEnd_0 = MEMCHR(pBegin, '>', pEnd - pBegin);
			char* pEnd_1 = MEMCHR(pBegin, ' ', pEnd - pBegin);

			if (pEnd_1 && pEnd_1 < pEnd_0)
			{
				pEnd_0 = pEnd_1;
			}

			char* pName = new char[pEnd_0 - pBegin];
			memcpy(pName, pBegin + 1, pEnd_0 - pBegin - 1);
			pName[pEnd_0 - pBegin - 1] = 0;

			CXMLNode* pNode = new CXMLNode(pName);
			if (!m_pMainNode)
			{
				m_pMainNode = pNode;
				pParentNode = pNode;
			}
			else if (pParentNode)
			{
				pParentNode->PushNode(pNode);
				pNode->SetParentNode(pParentNode);
			}

			char* pNewLine = MEMCHR(pEnd_0, '\n', pEnd - pEnd_0);
			char* pOpen = MEMCHR(pEnd_0, '<', pNewLine - pEnd_0);
			char* pClose = MEMCHR(pEnd_0, '>', pNewLine - pEnd_0);

			if (!pOpen && pEnd_0[0] != ' ')
			{
				pParentNode = pNode;
				pCurrent = pNewLine;
			}
			else
			{
				if (pOpen && pClose != pOpen)
				{
					char* pValue = new char[pOpen - pClose];
					memcpy(pValue, pClose + 1, pOpen - pClose);
					pValue[pOpen - pClose - 1] = 0;
					pNode->SetValue(pValue);
				}

				pCurrent = pNewLine;
			}
		}

		return true;
	}
};

#endif