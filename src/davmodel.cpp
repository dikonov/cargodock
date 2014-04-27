#include "davmodel.h"
#include "davapi/davfile.h"

#include <QEventLoop>
#include <QList>
#include <QUrl>
#include <QDebug>


DavModel::DavModel(QObject* parent)
    : FolderBase(parent)
    , myDavApi(new DavApi)
    , myMkColResult(DavApi::NoContent)
    , myDeleteResult(DavApi::NoContent)
    , myIsLoading(false)
{
    connect(myDavApi.data(), SIGNAL(propertiesReceived(DavApi::Properties)),
            this, SLOT(slotPropertiesReceived(DavApi::Properties)));
    connect(myDavApi.data(), SIGNAL(mkColFinished(int)),
            this, SLOT(slotMkColFinished(int)));
    connect(myDavApi.data(), SIGNAL(deleteFinished(int)),
            this, SLOT(slotDeleteFinished(int)));
}

DavModel::DavModel(const DavModel& other)
    : FolderBase(other)
    , myDavApi(new DavApi)
    , myMkColResult(DavApi::NoContent)
    , myDeleteResult(DavApi::NoContent)
    , myIsLoading(false)
{
    connect(myDavApi.data(), SIGNAL(propertiesReceived(DavApi::Properties)),
            this, SLOT(slotPropertiesReceived(DavApi::Properties)));
    connect(myDavApi.data(), SIGNAL(mkColFinished(int)),
            this, SLOT(slotMkColFinished(int)));
    connect(myDavApi.data(), SIGNAL(deleteFinished(int)),
            this, SLOT(slotDeleteFinished(int)));

    init();
}

FolderBase* DavModel::clone() const
{
    DavModel* dolly = new DavModel(*this);
    return dolly;
}

void DavModel::init()
{
    myDavApi->setAddress(configValue("url").toString());
}

QVariant DavModel::data(const QModelIndex& index, int role) const
{
    if (! index.isValid() || index.row() >= itemCount())
    {
        return QVariant();
    }

    switch (role)
    {
    case PermissionsRole:
        return FolderBase::ReadOwner | FolderBase::WriteOwner;
    default:
        return FolderBase::data(index, role);
    }
}

int DavModel::capabilities() const
{
    int caps = AcceptCopy;
    if (selected() > 0)
    {
        bool canBookmark = true;
        foreach (const QString& path, selection())
        {
            if (type(path) != Folder)
            {
                canBookmark = false;
                break;
            }
        }

        caps |= (canBookmark ? CanBookmark : NoCapabilities) |
                CanCopy |
                CanDelete;
    }
    return caps;
}

QString DavModel::friendlyBasename(const QString& path) const
{
    if (path == "/")
    {
        return "DAV";
    }
    else
    {
        return basename(path).toUtf8();
    }
}

QIODevice* DavModel::openFile(const QString& path,
                              QIODevice::OpenModeFlag mode)
{
    DavFile* fd = new DavFile(path, myDavApi);
    fd->open(mode);
    return fd;
}

bool DavModel::makeDirectory(const QString& path)
{
    myMkColResult = 0;
    myDavApi->mkcol(path);

    // this action needs to be synchronous, so wait for the response
    QEventLoop evLoop;
    while (myMkColResult == 0)
    {
        evLoop.processEvents();
    }
    return myMkColResult == DavApi::Created;
}

bool DavModel::deleteFile(const QString& path)
{
    qDebug() << Q_FUNC_INFO << path;
    myDeleteResult = 0;
    myDavApi->deleteResource(path);

    // this action needs to be synchronous, so wait for the response
    QEventLoop evLoop;
    while (myDeleteResult == 0)
    {
        evLoop.processEvents();
    }
    return myDeleteResult == DavApi::NoContent;
}

void DavModel::loadDirectory(const QString& path)
{
    qDebug() << Q_FUNC_INFO << path;
    clearItems();

    myIsLoading = true;
    emit loadingChanged();
    myDavApi->propfind(path);
}

void DavModel::slotPropertiesReceived(const DavApi::Properties& props)
{
    if (props.href.size())
    {
        Item::Ptr item(new Item);
        item->selectable = true;
        item->name = props.name.toUtf8();
        item->path = parentPath(props.href);
        item->uri = joinPath(QStringList() << path() << item->name);
        item->type = props.resourceType == "collection" ? Folder : File;
        item->mimeType = props.contentType.size() ? props.contentType
                                                  : "application/x-octet-stream";
        item->size = props.contentLength;
        item->icon = item->type == Folder ? "image://theme/icon-m-folder"
                                          : mimeTypeIcon(item->mimeType);
        item->mtime = props.lastModified;

        appendItem(item);

        qDebug() << props.name << props.contentType << props.contentLength
                    << props.resourceType << props.lastModified;
    }
    else
    {
        myIsLoading = false;
        emit loadingChanged();
    }
}

void DavModel::slotMkColFinished(int result)
{
    myMkColResult = result;
}

void DavModel::slotDeleteFinished(int result)
{
    myDeleteResult = result;
}
