#include "VersionCreater.h"
#include "VersionCreater_p.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStringList>
#include <QDomProcessingInstruction>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QDataStream>
#include <Filter.h>

VersionCreater::VersionCreater()
    : VersionBase(*new VersionCreaterPrivate)
{
}

VersionCreater::~VersionCreater()
{
}

bool VersionCreater::serializeFileNameAndFilePath(const QString &savePath)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);

    out << map_name_filePath_;

    QFile file(savePath);
    if(!file.open(QFile::WriteOnly))
    {
        qDebug()<< "open file fail when serialize data" << savePath;
        file.close();
        return false;
    }

    if(0 >= file.write(bytes))
    {
        qDebug() << "write data size less then zero";
        file.close();
        return false;
    }

    file.close();
    return true;
}

VersionCreater::VersionCreater(VersionCreaterPrivate &data)
    : VersionBase(data)
{
}

void VersionCreater::initDomTree()
{
    DPTR_D(VersionCreater);
    d.pDocument_ = new QDomDocument();
    QDomProcessingInstruction instruction = d.pDocument_->createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    d.pDocument_->appendChild(instruction);
    QDomElement updateElement = d.pDocument_->createElement("update");
    d.pDocument_->appendChild(updateElement);

    QDomElement fileElement = d.pDocument_->createElement("files");
    QDomElement ipAddressElement = d.pDocument_->createElement("serverAddress");
    updateElement.appendChild(fileElement);
    updateElement.appendChild(ipAddressElement);

    d.pUpdateNode_ = new QDomNode(d.pDocument_->firstChildElement("update"));
    d.pFilesNode_ = new QDomNode(d.pUpdateNode_->firstChildElement("files"));
    d.pIpAddressNode_= new QDomNode(d.pUpdateNode_->firstChildElement("serverAddress"));
    QDomText ipText = d.pDocument_->createTextNode(getHostIp());
    //QDomText ipText = d.pDocument_->createTextNode(getLocalIpAddress());
    d.pIpAddressNode_->appendChild(ipText);
    //qDebug()<< "ip address: " <<getLocalIpAddress();

    qDebug() << d.pUpdateNode_->nodeName();
    qDebug() << d.pFilesNode_->nodeName();
    qDebug() << d.pIpAddressNode_->nodeName();
}

void VersionCreater::traveDomTree(const QString &parentDirPath, const QStringList &filterFolderPaths)
{
    QDir dir(parentDirPath);
    dir.setFilter(QDir::Files);

    //先处理files
    foreach (const QString &fileName, dir.entryList())
    {
        QString path = parentDirPath + "/" + fileName;
        QFile file(path);

        if(!file.open(QFile::ReadOnly))
        {
            qDebug() << "can't open file:" << path;
            file.close();
            continue;
        }

        DPTR_D(VersionCreater);

        if(Filter::isFilterFiles(fileName))
        {
            qDebug() << "filter: " << fileName;
            file.close();
            continue;
        }

        ///////////////////////////////////////////
        //先存储记录文件名还有文件路径的map中
        {
            QFile file(path);
            QFileInfo fileInfor(file);

            QDir dir(fileInfor.path());
            if(dir.exists())
            {
                map_name_filePath_[fileName] = fileInfor.path();
            }
            else
            {
                qDebug() << "dir path not exists!";
            }
        }
        ////////////////////////////////////////////

        QDomNode filesNode = d.pDocument_->createElement("file");
        d.pFilesNode_->appendChild(filesNode);

        //<name></name>
        QDomNode fileNameNode = d.pDocument_->createElement("name");
        QDomText fileNameText = d.pDocument_->createTextNode(fileName);
        fileNameNode.appendChild(fileNameText);
        filesNode.appendChild(fileNameNode);

        //<path></path>
        QDomNode filePathNode = d.pDocument_->createElement("path");
        QDomText filePathText = d.pDocument_->createTextNode(path);
        filePathNode.appendChild(filePathText);
        filesNode.appendChild(filePathNode);

        //<md5></md5>
        QDomNode fileMD5Node = d.pDocument_->createElement("md5");
        QDomText fileMD5Text = d.pDocument_->createTextNode(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex());
        fileMD5Node.appendChild(fileMD5Text);
        filesNode.appendChild(fileMD5Node);

        //<lastModify></lastModify>
        QDomNode fileLastModifyNode = d.pDocument_->createElement("lastModify");
        QDomText fileLastModifyText = d.pDocument_->createTextNode(QFileInfo(path).lastModified().toString("yyyy-MM-dd,hh:mm"));
        fileLastModifyNode.appendChild(fileLastModifyText);
        filesNode.appendChild(fileLastModifyNode);
    }

    //再遍历子目录
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    foreach (const QString &dirName, dir.entryList())
    {
        QString childDirPath = parentDirPath + "/" + dirName;
        foreach (const QString &path, filterFolderPaths)
        {
            if(childDirPath.contains(path))
            {
                return;
            }
        }

        if(Filter::isFilterDir(childDirPath))
        {
            return;
        }

        traveDomTree(childDirPath, filterFolderPaths);
    }
}

QString VersionCreater::getLocalIpAddress()
{
    QString ipAddress = "";

#ifdef _WIN32
    QHostInfo localHostInfo = QHostInfo::fromName(QHostInfo::localHostName());
    QList<QHostAddress> ipAddressList = localHostInfo.addresses();
#else
    QList<QHostAddress> ipAddressList = QNetworkInterface::allAddresses();
#endif
    for(int i = 0; i < ipAddressList.size(); ++i)
    {
        if(!ipAddressList.at(i).isNull() &&
                ipAddressList.at(i) != QHostAddress::LocalHost &&
                ipAddressList.at(i).protocol() ==  QAbstractSocket::IPv4Protocol)
        {
            ipAddress = ipAddressList.at(i).toString();
            break;
        }
    }

    return ipAddress;
}

QString VersionCreater::getHostIp()
{
    qWarning()<<__FUNCTION__;
    QHostInfo hostInfo = QHostInfo::fromName(QHostInfo::localHostName());
    QList<QHostAddress> hostNameLookupAddressList = hostInfo.addresses();
    QList<QHostAddress> interfaceAddressList = QNetworkInterface::allAddresses();
    qDebug()<<__FUNCTION__<<"hostName lookup addresses:"<<hostNameLookupAddressList;
    qDebug()<<__FUNCTION__<<"interface addresses:"<<interfaceAddressList;

    QString hostIpStr;
    foreach(QHostAddress addr, hostNameLookupAddressList)
    {
        if(addr.protocol() == QAbstractSocket::IPv4Protocol && interfaceAddressList.contains(addr))
        {
            if(isLocalIp(addr))
            {
                qDebug()<<__FUNCTION__<<addr<<" is local ip";
                hostIpStr = addr.toString();
                break;
            }
            else if(isLinkLocalAddress(addr))
            {
                qDebug()<<__FUNCTION__<<addr<<" is Link Local Address";
                hostIpStr = addr.toString();
            }
        }
    }

    return hostIpStr;
}

/*!
 * \brief 判断是否是本地链接寻址(Autoip)
 * \param addr ip地址
 * \return bool
 */
bool VersionCreater::isLinkLocalAddress(QHostAddress addr)
{
    quint32 hostIpv4Addr = addr.toIPv4Address();
    quint32 rangeMin = QHostAddress("169.254.1.0").toIPv4Address();
    quint32 rangeMax = QHostAddress("169.254.254.255").toIPv4Address();
    if(hostIpv4Addr >= rangeMin && hostIpv4Addr <= rangeMax)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*!
 * \brief 判断是否是私有地址
 * \param addr 地址
 * \return bool
 */
bool VersionCreater::isLocalIp(QHostAddress addr)
{
    quint32 hostIpv4Addr = addr.toIPv4Address();
    //A类地址私有地址段
    quint32 range1Min = QHostAddress("10.0.0.0").toIPv4Address();
    quint32 range1Max = QHostAddress("10.255.255.255").toIPv4Address();
    //B类地址私有地址段
    quint32 range3Min = QHostAddress("172.16.0.0").toIPv4Address();
    quint32 range3Max = QHostAddress("172.31.255.255").toIPv4Address();
    //C类地址私有地址段
    quint32 range2Min = QHostAddress("192.168.0.0").toIPv4Address();
    quint32 range2Max = QHostAddress("192.168.255.255").toIPv4Address();

    if((hostIpv4Addr >= range1Min && hostIpv4Addr <= range1Max)
            || (hostIpv4Addr >= range2Min && hostIpv4Addr <= range2Max)
            || (hostIpv4Addr >= range3Min && hostIpv4Addr <= range3Max))
    {
        return true;
    }
    else
    {
        return false;
    }
}
