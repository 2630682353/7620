#include <errno.h>
#include <fstream>
#include "du_config.h"
#include "du_common.h"

DU_ConfigDomain::DU_ConfigDomain(const string &sLine)
{
	_name = DU_Common::trim(sLine);
}

DU_ConfigDomain::~DU_ConfigDomain()
{
	destroy();
}

DU_ConfigDomain::DU_ConfigDomain(const DU_ConfigDomain &tcd)
{
    (*this) = tcd;
}

DU_ConfigDomain& DU_ConfigDomain::operator=(const DU_ConfigDomain &tcd)
{
    if(this != &tcd)
    {
        destroy();

        _name  = tcd._name;
        _param = tcd._param;
        _key   = tcd._key;
        _domain= tcd._domain;

        const map<string, DU_ConfigDomain*> & m = tcd.getDomainMap();
        map<string, DU_ConfigDomain*>::const_iterator it = m.begin();
        while(it != m.end())
        {
            _subdomain[it->first] = it->second->clone();
            ++it;
        }
    }
    return *this;
}

DU_ConfigDomain::DomainPath DU_ConfigDomain::parseDomainName(const string& path, bool bWithParam)
{
    DU_ConfigDomain::DomainPath dp;

    if(bWithParam)
    {
    	string::size_type pos1 = path.find_first_of(DU_CONFIG_PARAM_BEGIN);
    	if(pos1 == string::npos)
    	{
    		throw DU_Config_Exception("[DU_Config::parseDomainName] : param path '" + path + "' is invalid!" );
    	}

    	if(path[0] != DU_CONFIG_DOMAIN_SEP)
    	{
    		throw DU_Config_Exception("[DU_Config::parseDomainName] : param path '" + path + "' must start with '/'!" );
    	}

    	string::size_type pos2 = path.find_first_of(DU_CONFIG_PARAM_END);
    	if(pos2 == string::npos)
    	{
    		throw DU_Config_Exception("[DU_Config::parseDomainName] : param path '" + path + "' is invalid!" );
    	}

        dp._domains = DU_Common::sepstr<string>(path.substr(1, pos1-1), DU_Common::tostr(DU_CONFIG_DOMAIN_SEP));
        dp._param = path.substr(pos1+1, pos2 - pos1 - 1);
    }
    else
    {
//    	if(path.length() <= 1 || path[0] != DU_CONFIG_DOMAIN_SEP)
        if(path[0] != DU_CONFIG_DOMAIN_SEP)
    	{
    		throw DU_Config_Exception("[DU_Config::parseDomainName] : param path '" + path + "' must start with '/'!" );
    	}

        dp._domains = DU_Common::sepstr<string>(path.substr(1), DU_Common::tostr(DU_CONFIG_DOMAIN_SEP));
    }

    return dp;
}

DU_ConfigDomain* DU_ConfigDomain::addSubDomain(const string& name)
{
    if(_subdomain.find(name) == _subdomain.end())
    {
        _domain.push_back(name);

        _subdomain[name] = new DU_ConfigDomain(name);
    }
    return _subdomain[name];
}

string DU_ConfigDomain::getParamValue(const string &name) const
{
    map<string, string>::const_iterator it = _param.find(name);
	if( it == _param.end())
	{
		throw DU_ConfigNoParam_Exception("[DU_ConfigDomain::getParamValue] param '" + name + "' not exits!");
    }

	return it->second;
}

DU_ConfigDomain *DU_ConfigDomain::getSubTcConfigDomain(vector<string>::const_iterator itBegin, vector<string>::const_iterator itEnd)
{
    if(itBegin == itEnd)
    {
        return this;
    }

    map<string, DU_ConfigDomain*>::const_iterator it = _subdomain.find(*itBegin);

	//根据匹配规则找不到匹配的子域
	if(it == _subdomain.end())
	{
		return NULL;
	}

	//继续在子域下搜索
	return it->second->getSubTcConfigDomain(itBegin + 1, itEnd);
}

const DU_ConfigDomain *DU_ConfigDomain::getSubTcConfigDomain(vector<string>::const_iterator itBegin, vector<string>::const_iterator itEnd) const
{
    if(itBegin == itEnd)
    {
        return this;
    }

    map<string, DU_ConfigDomain*>::const_iterator it = _subdomain.find(*itBegin);

	//根据匹配规则找不到匹配的子域
	if(it == _subdomain.end())
	{
		return NULL;
	}

	//继续在子域下搜索
	return it->second->getSubTcConfigDomain(itBegin + 1, itEnd);
}

void DU_ConfigDomain::insertParamValue(const map<string, string> &m)
{
    _param.insert(m.begin(),  m.end());

    map<string, string>::const_iterator it = m.begin();
    while(it != m.end())
    {
        size_t i = 0;
        for(; i < _key.size(); i++)
        {
            if(_key[i] == it->first)
            {
                break;
            }
        }

        //没有该key, 则添加到最后
        if(i == _key.size())
        {
            _key.push_back(it->first);
        }

        ++it;
    }
}

void DU_ConfigDomain::setParamValue(const string &name, const string &value)
{
    _param[name] = value;

    //如果key已经存在,则删除
    for(vector<string>::iterator it = _key.begin(); it != _key.end(); ++it)
    {
        if(*it == name)
        {
            _key.erase(it);
            break;
        }
    }

    _key.push_back(name);
}

void DU_ConfigDomain::setParamValue(const string &line)
{
    if(line.empty())
    {
        return;
    }

    _line.push_back(line);

    string::size_type pos = 0;
    for(; pos <= line.length() - 1; pos++)
    {
        if (line[pos] == '=')
        {
            if(pos > 0 && line[pos-1] == '\\')
            {
                continue;
            }

            string name  = parse(DU_Common::trim(line.substr(0, pos), " \r\n\t"));

            string value;
            if(pos < line.length() - 1)
            {
                value = parse(DU_Common::trim(line.substr(pos + 1), " \r\n\t"));
            }

            setParamValue(name, value);
            return;
        }
    }

    setParamValue(line, "");
}

string DU_ConfigDomain::parse(const string& s)
{
    if(s.empty())
    {
        return "";
    }

    string param;
    string::size_type pos = 0;
    for(; pos <= s.length() - 1; pos++)
    {
        char c;
        if(s[pos] == '\\' && pos < s.length() - 1)
        {
            switch (s[pos+1])
            {
            case '\\':
                c = '\\';
                pos++;
                break;
            case 'r':
                c = '\r';
                pos++;
                break;
            case 'n':
                c = '\n';
                pos++;
                break;
            case 't':
                c = '\t';
                pos++;
                break;
            case '=':
                c = '=';
                pos++;
                break;
            default:
                throw DU_Config_Exception("[DU_ConfigDomain::parse] '" + s + "' is invalid, '" + DU_Common::tostr(s[pos]) + DU_Common::tostr(s[pos+1]) + "' couldn't be parse!" );
            }

            param += c;
        }
        else if (s[pos] == '\\')
        {
            throw DU_Config_Exception("[DU_ConfigDomain::parse] '" + s + "' is invalid, '" + DU_Common::tostr(s[pos]) + "' couldn't be parse!" );
        }
        else
        {
            param += s[pos];
        }
    }

    return param;
}

string DU_ConfigDomain::reverse_parse(const string &s)
{
    if(s.empty())
    {
        return "";
    }

    string param;
    string::size_type pos = 0;
    for(; pos <= s.length() - 1; pos++)
    {
        string c;
        switch (s[pos])
        {
        case '\\':
            param += "\\\\";
            break;
        case '\r':
            param += "\\r";
            break;
        case '\n':
            param += "\\n";
            break;
        case '\t':
            param += "\\t";
            break;
            break;
        case '=':
            param += "\\=";
            break;
        case '<':
        case '>':
            throw DU_Config_Exception("[DU_ConfigDomain::reverse_parse] '" + s + "' is invalid, couldn't be parse!" );
        default:
            param += s[pos];
        }
    }

    return param;
}

string DU_ConfigDomain::getName() const
{
	return _name;
}

void DU_ConfigDomain::setName(const string& name)
{
    _name = name;
}

vector<string> DU_ConfigDomain::getKey() const
{
    return _key;
}

vector<string> DU_ConfigDomain::getLine() const
{
    return _line;
}

vector<string> DU_ConfigDomain::getSubDomain() const
{
    return _domain;
}

void DU_ConfigDomain::destroy()
{
    _param.clear();
    _key.clear();
    _line.clear();
    _domain.clear();

    map<string, DU_ConfigDomain*>::iterator it = _subdomain.begin();
    while(it != _subdomain.end())
    {
        delete it->second;
        ++it;
    }

    _subdomain.clear();
}

string DU_ConfigDomain::tostr(int i) const
{
    string sTab;
    for(int k = 0; k < i; ++k)
    {
        sTab += "\t";
    }

    ostringstream buf;

    buf << sTab << "<" << reverse_parse(_name) << ">" << endl;;

    for(size_t n = 0; n < _key.size(); n++)
    {
        map<string, string>::const_iterator it = _param.find(_key[n]);

        assert(it != _param.end());

        //值为空, 则不打印出=
        if(it->second.empty())
        {
            buf << "\t" << sTab << reverse_parse(_key[n]) << endl;
        }
        else
        {
            buf << "\t" << sTab << reverse_parse(_key[n]) << "=" << reverse_parse(it->second) << endl;
        }
    }

    ++i;

    for(size_t n = 0; n < _domain.size(); n++)
    {
        map<string, DU_ConfigDomain*>::const_iterator itm = _subdomain.find(_domain[n]);

        assert(itm != _subdomain.end());

        buf << itm->second->tostr(i);
    }


	buf << sTab << "</" << reverse_parse(_name) << ">" << endl;

	return buf.str();
}

/********************************************************************/
/*		DU_Config implement										    */
/********************************************************************/

DU_Config::DU_Config() : _root("")
{
}

DU_Config::DU_Config(const DU_Config &tc)
: _root(tc._root)
{

}

DU_Config& DU_Config::operator=(const DU_Config &tc)
{
    if(this != &tc)
    {
        _root = tc._root;
    }

    return *this;
}

void DU_Config::parse(istream &is)
{
    _root.destroy();

    stack<DU_ConfigDomain*> stkTcCnfDomain;
    stkTcCnfDomain.push(&_root);

    string line;
    while(getline(is, line))
	{
		line = DU_Common::trim(line, " \r\n\t");

		if(line.length() == 0)
		{
			continue;
		}

		if(line[0] == '#')
		{
			continue;
		}
		else if(line[0] == '<')
		{
			string::size_type posl = line.find_first_of('>');

			if(posl == string::npos)
			{
				throw DU_Config_Exception("[DU_Config::parse]:parse error! line : " + line);
			}

			if(line[1] == '/')
			{
				string sName(line.substr(2, (posl - 2)));

				if(stkTcCnfDomain.size() <= 0)
                {
					throw DU_Config_Exception("[DU_Config::parse]:parse error! <" + sName + "> hasn't matched domain.");
                }

                if(stkTcCnfDomain.top()->getName() != sName)
				{
					throw DU_Config_Exception("[DU_Config::parse]:parse error! <" + stkTcCnfDomain.top()->getName() + "> hasn't match <" + sName +">.");
				}

                //弹出
				stkTcCnfDomain.pop();
			}
			else
			{
				string name(line.substr(1, posl - 1));

                stkTcCnfDomain.push(stkTcCnfDomain.top()->addSubDomain(name));
			}
		}
		else
		{
            stkTcCnfDomain.top()->setParamValue(line);
		}
	}

	if(stkTcCnfDomain.size() != 1)
	{
		throw DU_Config_Exception("[DU_Config::parse]:parse error : hasn't match");
    }
}

void DU_Config::parseFile(const string &sFileName)
{
    if(sFileName.length() == 0)
    {
		throw DU_Config_Exception("[DU_Config::parseFile]:file name is empty");
    }

    ifstream ff;
    ff.open(sFileName.c_str());
	if (!ff)
	{
		throw DU_Config_Exception("[DU_Config::parseFile]:fopen fail: " + sFileName, errno);
    }

    parse(ff);
}

void DU_Config::parseString(const string& buffer)
{
    istringstream iss;
    iss.str(buffer);

    parse(iss);
}

string DU_Config::operator[](const string &path)
{
    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(path, true);

	DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain == NULL)
	{
		throw DU_ConfigNoParam_Exception("[DU_Config::operator[]] path '" + path + "' not exits!");
	}

	return pTcConfigDomain->getParamValue(dp._param);
}

string DU_Config::get(const string &sName, const string &sDefault) const
{
    try
    {
        DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(sName, true);

    	const DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    	if(pTcConfigDomain == NULL)
    	{
    		throw DU_ConfigNoParam_Exception("[DU_Config::get] path '" + sName + "' not exits!");
    	}

    	return pTcConfigDomain->getParamValue(dp._param);
    }
    catch ( DU_ConfigNoParam_Exception &ex )
    {
        return sDefault;
    }
}

bool DU_Config::getDomainMap(const string &path, map<string, string> &m) const
{
    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(path, false);

	const DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain == NULL)
	{
		return false;
	}

	m = pTcConfigDomain->getParamMap();

    return true;
}

map<string, string> DU_Config::getDomainMap(const string &path) const
{
    map<string, string> m;

    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(path, false);

	const DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain != NULL)
	{
        m = pTcConfigDomain->getParamMap();
    }

    return m;
}

vector<string> DU_Config::getDomainKey(const string &path) const
{
    vector<string> v;

    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(path, false);

	const DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain != NULL)
	{
        v = pTcConfigDomain->getKey();
    }

    return v;
}

vector<string> DU_Config::getDomainLine(const string &path) const
{
    vector<string> v;

    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(path, false);

    const DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain != NULL)
    {
        v = pTcConfigDomain->getLine();
    }

    return v;
}

bool DU_Config::getDomainVector(const string &path, vector<string> &vtDomains) const
{
    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(path, false);

    //根域, 特殊处理
    if(dp._domains.empty())
    {
        vtDomains = _root.getSubDomain();
        return !vtDomains.empty();
    }

	const DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain == NULL)
	{
        return false;
	}

    vtDomains = pTcConfigDomain->getSubDomain();

	return true;
}

vector<string> DU_Config::getDomainVector(const string &path) const
{
    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(path, false);

    //根域, 特殊处理
    if(dp._domains.empty())
    {
        return _root.getSubDomain();
    }

	const DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain == NULL)
	{
        return vector<string>();
	}

    return pTcConfigDomain->getSubDomain();
}


DU_ConfigDomain *DU_Config::newTcConfigDomain(const string &sName)
{
	return new DU_ConfigDomain(sName);
}

DU_ConfigDomain *DU_Config::searchTcConfigDomain(const vector<string>& domains)
{
	return _root.getSubTcConfigDomain(domains.begin(), domains.end());
}

const DU_ConfigDomain *DU_Config::searchTcConfigDomain(const vector<string>& domains) const
{
	return _root.getSubTcConfigDomain(domains.begin(), domains.end());
}

int DU_Config::insertDomain(const string &sCurDomain, const string &sAddDomain, bool bCreate)
{
    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(sCurDomain, false);

	DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain == NULL)
	{
        if(bCreate)
        {
            pTcConfigDomain = &_root;

            for(size_t i = 0; i < dp._domains.size(); i++)
            {
                pTcConfigDomain = pTcConfigDomain->addSubDomain(dp._domains[i]);
            }
        }
        else
        {
            return -1;
        }
	}

    pTcConfigDomain->addSubDomain(sAddDomain);

    return 0;
}

int DU_Config::insertDomainParam(const string &sCurDomain, const map<string, string> &m, bool bCreate)
{
    DU_ConfigDomain::DomainPath dp = DU_ConfigDomain::parseDomainName(sCurDomain, false);

	DU_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

	if(pTcConfigDomain == NULL)
	{
        if(bCreate)
        {
            pTcConfigDomain = &_root;

            for(size_t i = 0; i < dp._domains.size(); i++)
            {
                pTcConfigDomain = pTcConfigDomain->addSubDomain(dp._domains[i]);
            }
        }
        else
        {
            return -1;
        }
	}

    pTcConfigDomain->insertParamValue(m);

    return 0;
}

string DU_Config::tostr() const
{
    string buffer;

    map<string, DU_ConfigDomain*> msd = _root.getDomainMap();
    map<string, DU_ConfigDomain*>::const_iterator it = msd.begin();
    while (it != msd.end())
    {
        buffer += it->second->tostr(0);
        ++it;
    }

    return buffer;
}

void DU_Config::joinConfig(const DU_Config &cf, bool bUpdate)
{
    string buffer;
    if(bUpdate)
    {
        buffer = tostr() + cf.tostr();
    }
    else
    {
        buffer = cf.tostr() + tostr();
    }
    parseString(buffer);
}

