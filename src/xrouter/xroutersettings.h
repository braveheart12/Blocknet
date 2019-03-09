//******************************************************************************
//******************************************************************************
#ifndef XROUTERSETTINGS_H
#define XROUTERSETTINGS_H

#include "xrouterpacket.h"
#include "xrouterdef.h"

#include "sync.h"

#include <vector>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/container/map.hpp>
#include <boost/algorithm/string.hpp>

#define TRY(_STMNT_) try { (_STMNT_); } catch(std::exception & e) { LOG() << e.what(); }

namespace xrouter
{
    
class IniConfig
{
public:
    IniConfig() = default;

    virtual bool read(const char * fileName = nullptr);
    virtual bool read(std::string config);
    virtual bool write(const char * fileName = nullptr);
    
    std::string rawText() const {
        LOCK(mu);
        return rawtext;
    }
    
    template <class _T>
    _T get(const std::string & param, _T def = _T())
    {
        return get<_T>(param.c_str(), def);
    }

    template <class _T>
    _T get(const char * param, _T def = _T())
    {
        LOCK(mu);
        _T tmp = def;
        try
        {
            tmp = m_pt.get<_T>(param);
            return tmp;
        }
        catch (std::exception & e)
        {
            //LOG() << e.what();
        }

        return tmp;
    }
    
    template <class _T>
    _T set(const std::string & param, _T def = _T())
    {
        return set<_T>(param.c_str(), def);
    }
    
    template <class _T>
    bool set(const char * param, const _T & val)
    {
        try
        {
            {
                LOCK(mu);
                m_pt.put<_T>(param, val);
            }
            write();
        }
        catch (std::exception & e)
        {
            return false;
        }
        return true;
    }
    
protected:
    std::string m_fileName;
    boost::property_tree::ptree m_pt;
    std::string rawtext;
    mutable CCriticalSection mu;
};

class XRouterPluginSettings;
typedef std::shared_ptr<XRouterPluginSettings> XRouterPluginSettingsPtr;
class XRouterPluginSettings : public IniConfig
{
public:
    XRouterPluginSettings() = default;

    std::string getParam(std::string param, std::string def="");
    double getFee();
    int minParamCount();
    int maxParamCount();
    int clientRequestLimit();
    int commandTimeout();

    std::string rawText() {
        LOCK(mu);
        return publictext;
    }

    bool read(const char * fileName) override;
    bool read(std::string config) override;
    
    bool verify(std::string name="");
private:
    void formPublicText();
    std::string publictext;
};

//******************************************************************************
class XRouterSettings : public IniConfig
{
public:
    XRouterSettings() = default;
    XRouterSettings(const std::string & config);

    void assignNode(const std::string & node) {
        LOCK(mu);
        this->node = node;
    }
    const std::string & getNode() {
        LOCK(mu);
        return this->node;
    }

    void loadWallets();
    std::vector<std::string> getWallets() {
        LOCK(mu);
        return {wallets.begin(), wallets.end()};
    }
    bool hasWallet(const std::string & currency) {
        LOCK(mu);
        return wallets.count(currency);
    }

    void loadPlugins();

    std::vector<std::string> getPlugins() {
        LOCK(mu);
        return {pluginList.begin(),pluginList.end()};
    }

    std::string pluginPath() const;

    void addPlugin(const std::string &name, XRouterPluginSettingsPtr s) {
        LOCK(mu);
        plugins[name] = s; pluginList.insert(name);
    }

    bool hasPlugin(std::string name);

    XRouterPluginSettingsPtr getPluginSettings(const std::string & name) {
        LOCK(mu);
        return plugins[name];
    }

    bool walletEnabled(std::string & currency);
    bool isAvailableCommand(XRouterCommand c, std::string currency="");
    double getCommandFee(XRouterCommand c, std::string currency="", double def=0.0);
    int commandTimeout(XRouterCommand c, std::string currency="", int def=XROUTER_DEFAULT_TIMEOUT);
    int getCommandBlockLimit(XRouterCommand c, std::string currency="", int def=XROUTER_DEFAULT_BLOCK_LIMIT);
    double getMaxFee(XRouterCommand c, std::string currency="", double def=0.0);
    int clientRequestLimit(XRouterCommand c, std::string currency="", int def=-1); // -1 is no limit
    int configSyncTimeout();

    double defaultFee();
    std::map<std::string, double> getFeeSchedule() {
        LOCK(mu);

        double fee = defaultFee();
        std::map<std::string, double> s;

        // First pass set top-level fees
        for (const auto & p : m_pt) {
            std::vector<std::string> parts;
            boost::split(parts, p.first, boost::is_any_of("::"));
            std::string cmd = parts[0];

            if (parts.size() > 1)
                continue; // skip currency fees (addressed in 2nd pass below)
            if (boost::algorithm::to_lower_copy(cmd) == "main")
                continue; // skip Main

            s[p.first] = m_pt.get<double>(p.first + ".fee", fee);
        }

        // 2nd pass to set currency fees
        for (const auto & p : m_pt) {
            if (s.count(p.first))
                continue; // skip existing

            std::vector<std::string> parts;
            boost::split(parts, p.first, boost::is_any_of("::"));

            if (parts.size() < 3)
                continue; // skip

            std::string currency = parts[0];
            std::string cmd = parts[2];

            s[p.first] = m_pt.get<double>(p.first + ".fee", s.count(cmd) ? s[cmd] : fee); // default to top-level fee
        }

        return s;
    }

private:
    bool loadPlugin(std::string & name);

private:
    std::map<std::string, XRouterPluginSettingsPtr> plugins;
    std::set<std::string> pluginList;
    std::set<std::string> wallets;
    std::string node;
};

} // namespace

#endif // SETTINGS_H
