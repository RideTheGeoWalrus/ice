// **********************************************************************
//
// Copyright (c) 2001
// Mutable Realms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

#ifndef ICE_PACK_SERVICE_BUILDER_H
#define ICE_PACK_SERVICE_BUILDER_H

#include <IcePack/ComponentBuilder.h>
#include <IcePack/NodeInfo.h>

namespace IcePack
{

class ServerBuilder;

class ServiceBuilder : public ComponentBuilder
{
public:

    enum ServiceKind
    {
	ServiceKindStandard,
	ServiceKindFreeze
    };

    ServiceBuilder(const NodeInfoPtr&, ServerBuilder&, 
		   const std::map<std::string, std::string>&,
		   const std::vector<std::string>&);

    void parse(const std::string&);

    ServerBuilder& getServerBuilder() const;

    void setKind(ServiceKind);
    void setEntryPoint(const std::string&);
    void setDBEnv(const std::string&);

    virtual std::string getDefaultAdapterId(const std::string&);

private:

    NodeInfoPtr _nodeInfo;

    ServerBuilder& _serverBuilder;

    ServiceKind _kind;
};

}

#endif
