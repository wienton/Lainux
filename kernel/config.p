/* Configuration for build lainux ISO */


struct SystemConfig {
    float Version
    String systemName
    String Description
};

function connectNerwork : bool ()
{
    bool connect = new ConnectNetwork("127.0.0.1", "8080");

    if(connect) {

        return PrintError(ErrorNetwork);

    }

    PrintLine("error connect by $full(%date)" + connect);
    getName();
}

function getName : void ()
{
    SystemConfig sysConfig = new SystemConfig(struct);

    PrintLine(sysConfig.systemName);
    PrintDefault(sysConfig.Version);

}



