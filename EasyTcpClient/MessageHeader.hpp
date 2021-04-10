#ifndef _MessageHeader_Hpp_
#define _MessageHeader_Hpp_

enum CMD
{
    CMD_LOGIN,
    CMD_LOGOUT,
    CMD_LOGRESULT,
    CMD_LOGOUTRES,
    CMD_NEW_USER_JOIN,
    CMD_ERROR
};

//包头
struct DataHeader
{
    DataHeader()
    {
        dataLength = sizeof(DataHeader);
        cmd = CMD_ERROR;
    }
    short dataLength;
    short cmd;
};

//包体
struct Login : public DataHeader
{
    Login()
    {
        dataLength = sizeof(Login);
        cmd = CMD_LOGIN;
    }
    char userName[32];
    char PassWord[32];
    char data[32];
};

struct LogResult : public DataHeader
{
    LogResult()
    {
        dataLength = sizeof(LogResult);
        cmd = CMD_LOGRESULT;
        result = 1;
    }
    int result;
    char data[92];
};

struct LogOut : public DataHeader
{
    LogOut()
    {
        dataLength = sizeof(LogOut);
        cmd = CMD_LOGOUT;
    }
    char userName[32];
};

struct LogOutRes : public DataHeader
{
    LogOutRes()
    {
        dataLength = sizeof(LogOutRes);
        cmd = CMD_LOGOUTRES;
        result = 1;
    }
    int result;
};

struct NewUserJoin : public DataHeader
{
    NewUserJoin()
    {
        dataLength = sizeof(NewUserJoin);
        cmd = CMD_NEW_USER_JOIN;
        Sock = 0;
    }
    int Sock;
};
#endif //_MessageHeader_Hpp_

