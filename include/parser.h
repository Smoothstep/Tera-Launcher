#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>

#include <ostream>
#include <fstream>

#define NEW (char*)(malloc)
#define CPY (char*)(memcpy)

static inline char * strndup(const char *s, size_t n)
{
	size_t len = strlen(s);

	if (n < len)
	{
		len = n;
	}

	char* result = NEW(len + 1);

	if (!result)
	{
		return 0;
	}

	result[len] = '\0';

	return CPY(result, s, len);
}


class CPairNode : public std::vector<CPairNode*>
{
private:
	char* m_pValue, *m_pName;
	CPairNode* m_pParentNode;

public:
	CPairNode() :
		m_pParentNode(NULL),
		m_pValue(NULL),
		m_pName(NULL) {}

	CPairNode(const char* name) :
		m_pParentNode(NULL),
		m_pValue(NULL),
		m_pName(const_cast<char*>(name)) {}

	CPairNode(const char* name, const char* value) :
		m_pParentNode(NULL),
		m_pValue(const_cast<char*>(value)),
		m_pName(const_cast<char*>(name)) {}

	~CPairNode()
	{
		if (m_pName)
		{
			free(m_pName);
		}

		if (m_pValue)
		{
			free(m_pValue);
		}

		for (CPairNode::iterator it = begin(); it != end(); ++it)
		{
			delete *it;
		}
	}

	inline CPairNode* GetNode(const char* name)
	{
		for (CPairNode::iterator it = begin(); it != end(); ++it)
		{
			if (strcmp((*it)->GetName(), name) == 0)
			{
				return *it;
			}
		}

		return NULL;
	}

	inline CPairNode* GetParentNode()
	{
		return m_pParentNode;
	}

	inline const char* GetName()
	{
		return m_pName;
	}

	inline const char* GetValue()
	{
		return m_pValue;
	}

	template<typename T> inline T GetValue()
	{
		T value;

		try
		{
			value = boost::lexical_cast<T>(m_pValue);
		}
		catch (...)
		{
			return T();
		}

		return value;
	}

	template<typename T> inline bool GetValue(T& value)
	{
		try
		{
			value = boost::lexical_cast<T>(m_pValue);
		}
		catch (...)
		{
			return false;
		}

		return true;
	}

	inline void SetParentNode(CPairNode* pParentNode)
	{
		m_pParentNode = pParentNode;
	}

	inline void PushNode(CPairNode* pNode)
	{
		push_back(pNode);
	}

	inline void SetValue(const char* pValue)
	{
		if (m_pValue)
		{
			free(m_pValue);
		}

		m_pValue = const_cast<char*>(pValue);
	}

	inline void SetValueCopy(const char* pValue)
	{
		if (m_pValue)
		{
			free(m_pValue);
		}

		m_pValue = _strdup(pValue);
	}

	inline void SetName(const char* pName)
	{
		if (m_pName)
		{
			free(m_pName);
		}

		m_pName = const_cast<char*>(pName);
	}

	inline void SetNameCopy(const char* pName)
	{
		if (m_pName)
		{
			free(m_pName);
		}

		m_pName = _strdup(pName);
	}
};

#define MEMCHR(x,y,z) (char*)(memchr(x,y,z))

class CXMLDocument
{
private:
	CPairNode* m_pMainNode;

public:
	inline CPairNode* Node()
	{
		return m_pMainNode;
	}

public:
	CXMLDocument() :
		m_pMainNode(NULL) {}

	~CXMLDocument()
	{
		Clear();
	}

	void Clear()
	{
		if (m_pMainNode)
		{
			delete m_pMainNode;
			m_pMainNode = NULL;
		}
	}

	bool ReadXML(std::istream& stream)
	{
		return ReadXML(static_cast<std::stringstream&>(std::stringstream() 
			<< stream.rdbuf()).str().c_str());
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

		if (!iSize)
		{
			return false;
		}

		Clear();

		char* pCurrent = MEMCHR(pData, '\n', iSize) + 1;
		char* pEnd = (char*)pData + iSize;

		if (!pCurrent)
		{
			return false;
		}

		CPairNode* pParentNode = NULL;

		while (pCurrent)
		{
			char* pBegin = MEMCHR(pCurrent, '<', iSize);

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

				if (!(pCurrent = MEMCHR(pCurrent + 1, '\n', pEnd - pBegin)))
				{
					break;
				}

				pCurrent++;

				continue;
			}

			char* pEnd_0 = MEMCHR(pBegin, '>', pEnd - pBegin);
			char* pEnd_1 = MEMCHR(pBegin, ' ', pEnd - pBegin);

			if (pEnd_1 && pEnd_1 < pEnd_0)
			{
				pEnd_0 = pEnd_1;
			}

			CPairNode* pNode = new CPairNode(strndup(pBegin + 1, pEnd_0 - pBegin - 1));

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
					pNode->SetValue(strndup(pClose + 1, pOpen - pClose - 1));
				}

				pCurrent = pNewLine;
			}
		}

		return true;
	}
};

class CInitializationDocument
{
private:
	std::vector<CPairNode*> m_vNodes;

public:
	~CInitializationDocument()
	{
		Clear();
	}

	void Clear()
	{
		for (CPairNode::iterator it = m_vNodes.begin(); it != m_vNodes.end(); ++it)
		{
			delete *it;
		}

		m_vNodes.clear();
	}

	bool ParseIni(std::istream& stream)
	{
		return ParseIni(static_cast<std::stringstream&>(std::stringstream() 
			<< stream.rdbuf()).str().c_str());
	}

	bool ParseIni(const char* pData, size_t iSize = 0)
	{
		if (!pData)
		{
			return false;
		}

		if (!iSize)
		{
			iSize = strlen(pData);
		}

		if (!iSize)
		{
			return false;
		}

		Clear();

		char* pCurrent = (char*)pData;
		char* pNext, *pEqual;

		while (pCurrent[0])
		{
			pEqual = strchr(pCurrent, '=');

			if (!pEqual)
			{
				return false;
			}

			pNext = strchr(pCurrent, '\n');

			if (pNext && pEqual > pNext)
			{
				return false;
			}

			if (pEqual != pCurrent)
			{
				char* pName = strndup(pCurrent, pEqual - pCurrent);
				char* pValue = 0;

				if (pEqual + 1 != pNext)
				{
					size_t iLen = pNext ? pNext - pEqual : pData + iSize - pEqual;

					iLen = pEqual[iLen - 1] == '\r' ? iLen - 1 : iLen;

					if (iLen)
					{
						pValue = strndup(pEqual + 1, iLen - 1);
					}
				}

				m_vNodes.push_back(new CPairNode(pName, pValue));
			}

			if (!pNext)
			{
				break;
			}

			pCurrent = pNext + 1;
		}

		return true;
	}

	void ChangeNode(const char* pName, const char* pValue)
	{
		CPairNode* pNode = GetNode(pName);

		if (pNode)
		{
			pNode->SetValueCopy(pValue);
		}
		else
		{
			m_vNodes.push_back(new CPairNode(pName, pValue));
		}
	}

	CPairNode* GetNode(const char* name)
	{
		for (CPairNode::iterator it = m_vNodes.begin(); it != m_vNodes.end(); ++it)
		{
			if (strcmp((*it)->GetName(), name) == 0)
			{
				return *it;
			}
		}

		return NULL;
	}

	bool WriteIni(const char* pFileName)
	{
		if (!pFileName)
		{
			return false;
		}

		std::ofstream out(pFileName);

		if (!out.is_open())
		{
			return false;
		}

		for (CPairNode::iterator it = m_vNodes.begin(); it != m_vNodes.end(); ++it)
		{
			CPairNode* pNode = *it;

			if (pNode)
			{
				out << pNode->GetName() << "=";

				if (pNode->GetValue())
				{
					out << pNode->GetValue();
				}

				if (it + 1 != m_vNodes.end())
				{
					out << "\n";
				}
			}
		}

		return true;
	}
};

#endif