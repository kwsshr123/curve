/*
 * Project: curve
 * Created Date: Wednesday September 26th 2018
 * Author: hzsunjianliang
 * Copyright (c) 2018 netease
 */

#ifndef TEST_MDS_NAMESERVER2_FAKES_H_
#define TEST_MDS_NAMESERVER2_FAKES_H_

#include <glog/logging.h>
#include <vector>
#include <mutex>   // NOLINT
#include <utility>
#include <map>
#include <string>
#include "src/mds/common/mds_define.h"
#include "src/mds/nameserver2/inode_id_generator.h"
#include "src/mds/nameserver2/chunk_allocator.h"
#include "src/mds/nameserver2/namespace_storage.h"
#include "src/mds/nameserver2/chunk_id_generator.h"
#include "src/mds/nameserver2/session.h"
#include "src/mds/dao/mdsRepo.h"
#include "src/mds/topology/topology_admin.h"

using ::curve::mds::topology::TopologyAdmin;

const uint64_t FACK_INODE_INITIALIZE = 0;
const uint64_t FACK_CHUNKID_INITIALIZE = 0;

namespace curve {
namespace mds {
class FakeInodeIDGenerator : public InodeIDGenerator {
 public:
    explicit FakeInodeIDGenerator(uint64_t initValue = FACK_INODE_INITIALIZE) {
        value_ = initValue;
    }
    bool GenInodeID(InodeID *id) {
        *id = ++value_;
        return true;
    }
 private:
    std::atomic<uint64_t> value_;
};

class FackChunkIDGenerator: public ChunkIDGenerator {
 public:
    explicit FackChunkIDGenerator(uint64_t val = FACK_CHUNKID_INITIALIZE) {
        value_ = val;
    }
    bool GenChunkID(ChunkID *id) override {
        *id = ++value_;
        return true;
    }
 private:
    std::atomic<uint64_t> value_;
};

class FackTopologyAdmin: public TopologyAdmin {
 public:
    using CopysetIdInfo = ::curve::mds::topology::CopysetIdInfo;
    using FileType = ::curve::mds::FileType;

    FackTopologyAdmin() {}

    bool AllocateChunkRandomInSingleLogicalPool(
            FileType fileType, uint32_t chunkNumer,
            std::vector<CopysetIdInfo> *infos) override {
        for (uint32_t i = 0; i != chunkNumer; i++) {
            CopysetIdInfo copysetIdInfo{0, i};
            infos->push_back(copysetIdInfo);
        }
        return true;
    }
};

class FakeNameServerStorage : public NameServerStorage {
 public:
    StoreStatus PutFile(const std::string & storeKey,
          const FileInfo & fileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);

        auto iter = memKvMap_.find(storeKey);
        if (iter != memKvMap_.end()) {
            memKvMap_.erase(iter);
        }

        std::string value = fileInfo.SerializeAsString();
        memKvMap_.insert(
            std::move(std::pair<std::string, std::string>
            (storeKey, std::move(value))));
        return StoreStatus::OK;
    }

    StoreStatus GetFile(const std::string & storeKey,
          FileInfo * fileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storeKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        fileInfo->ParseFromString(iter->second);
        return StoreStatus::OK;
    }

    StoreStatus DeleteFile(const std::string & storekey) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storekey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        memKvMap_.erase(iter);
        return StoreStatus::OK;
    }

    StoreStatus RenameFile(const std::string & oldStoreKey,
                           const FileInfo &oldfileInfo,
                           const std::string & newStoreKey,
                           const FileInfo &newfileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);

        auto iter = memKvMap_.find(oldStoreKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        memKvMap_.erase(iter);

        std::string value = newfileInfo.SerializeAsString();
        memKvMap_.insert(
            std::move(std::pair<std::string, std::string>
            (newStoreKey, std::move(value))));

        return StoreStatus::OK;
    }

    StoreStatus ListFile(const std::string & startStoreKey,
                         const std::string & endStoreKey,
                         std::vector<FileInfo> * files) override {
        std::lock_guard<std::mutex> guard(lock_);

        for (auto iter = memKvMap_.begin(); iter != memKvMap_.end(); iter++) {
            if (iter->first.compare(startStoreKey) >= 0) {
                if (iter->first.compare(endStoreKey) < 0) {
                    FileInfo  validFile;
                    validFile.ParseFromString(iter->second);
                    files->push_back(validFile);
                }
            }
        }

        return StoreStatus::OK;
    }


    StoreStatus GetSegment(const std::string & storeKey,
                           PageFileSegment *segment) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storeKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        segment->ParseFromString(iter->second);
        return StoreStatus::OK;
    }

    StoreStatus PutSegment(const std::string & storeKey,
                           const PageFileSegment * segment) override {
        std::lock_guard<std::mutex> guard(lock_);
        std::string value = segment->SerializeAsString();
        memKvMap_.insert(std::move(std::pair<std::string, std::string>
            (storeKey, std::move(value))));
        return StoreStatus::OK;
    }

    StoreStatus DeleteSegment(const std::string &storeKey) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storeKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        memKvMap_.erase(iter);
        return StoreStatus::OK;
    }

    StoreStatus SnapShotFile(const std::string & originalFileKey,
                            const FileInfo *originalFileInfo,
                            const std::string & snapshotFileKey,
                            const FileInfo * snapshotFileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);
        std::string originalFileData = originalFileInfo->SerializeAsString();
        std::string snapshotFileData = snapshotFileInfo->SerializeAsString();
        memKvMap_.erase(originalFileKey);
        memKvMap_.insert(std::move(std::pair<std::string, std::string>
            (originalFileKey, std::move(originalFileData))));
        memKvMap_.insert(std::move(std::pair<std::string, std::string>
            (snapshotFileKey, std::move(snapshotFileData))));
        return StoreStatus::OK;
    }

    StoreStatus LoadSnapShotFile(std::vector<FileInfo> *snapShotFiles)
    override {
        std::lock_guard<std::mutex> guard(lock_);
        std::string snapshotStartKey = SNAPSHOTFILEINFOKEYPREFIX;

        for (auto iter = memKvMap_.begin(); iter != memKvMap_.end(); iter++) {
            if ( iter->first.length() > snapshotStartKey.length() ) {
                if (iter->first.substr(0, snapshotStartKey.length()).
                    compare(snapshotStartKey) == 0) {
                    FileInfo  validFile;
                    validFile.ParseFromString(iter->second);
                    snapShotFiles->push_back(validFile);
                }
            }
        }
        return StoreStatus::OK;
    }

 private:
    std::mutex lock_;
    std::map<std::string, std::string> memKvMap_;
};

class FakeRepoInterface : public MdsRepo {
 public:
    int connectDB(const std::string &dbName,
                        const std::string &user,
                        const std::string &url,
                        const std::string &password) override {
        return repo::OperationOK;
    }

    int createAllTables() override {
        return repo::OperationOK;
    }

    int createDatabase() override {
        return repo::OperationOK;
    }

    int useDataBase() override {
        return repo::OperationOK;
    }

    int dropDataBase() override {
        return repo::OperationOK;
    }

    // chunkServerRepo operation
    int InsertChunkServerRepoItem(const ChunkServerRepoItem &cr) override {
        return repo::OperationOK;
    }

    int LoadChunkServerRepoItems(
        std::vector<ChunkServerRepoItem> *chunkServerRepoList) override {
        return repo::OperationOK;
    }

    int DeleteChunkServerRepoItem(ChunkServerIDType id) override {
        return repo::OperationOK;
    }

    int UpdateChunkServerRepoItem(const ChunkServerRepoItem &cr) override {
        return repo::OperationOK;
    }

    int QueryChunkServerRepoItem(ChunkServerIDType id,
                                    ChunkServerRepoItem *repo) override {
        return repo::OperationOK;
    }
    // server operation
    int InsertServerRepoItem(const ServerRepoItem &sr) override {
        return repo::OperationOK;
    }

    int LoadServerRepoItems(std::vector<ServerRepoItem> *serverList) override {
        return repo::OperationOK;
    }

    int DeleteServerRepoItem(ServerIDType id) override {
        return repo::OperationOK;
    }

    int UpdateServerRepoItem(const ServerRepoItem &sr) override {
        return repo::OperationOK;
    }

    int QueryServerRepoItem(ServerIDType id, ServerRepoItem *repo)
                                                                override {
        return repo::OperationOK;
    }

    // zone operation
    int InsertZoneRepoItem(const ZoneRepoItem &zr) override {
        return repo::OperationOK;
    }

    int LoadZoneRepoItems(std::vector<ZoneRepoItem> *zonevector) override {
        return repo::OperationOK;
    }

    int DeleteZoneRepoItem(ZoneIDType id) {
        return repo::OperationOK;
    }

    int UpdateZoneRepoItem(const ZoneRepoItem &zr) override {
        return repo::OperationOK;
    }

    int QueryZoneRepoItem(ZoneIDType id, ZoneRepoItem *repo) override {
        return repo::OperationOK;
    }

    // physical pool operation
    int InsertPhysicalPoolRepoItem(const PhysicalPoolRepoItem &pr) override {
        return repo::OperationOK;
    }

    int LoadPhysicalPoolRepoItems(
        std::vector<PhysicalPoolRepoItem> *physicalPoolvector) override {
        return repo::OperationOK;
    }

    int DeletePhysicalPoolRepoItem(PhysicalPoolIDType id) override {
        return repo::OperationOK;
    }

    int UpdatePhysicalPoolRepoItem(const PhysicalPoolRepoItem &pr) override {
        return repo::OperationOK;
    }

    int QueryPhysicalPoolRepoItem(PhysicalPoolIDType id,
                                    PhysicalPoolRepoItem *repo) override {
        return repo::OperationOK;
    }

    // logical pool operation
    int InsertLogicalPoolRepoItem(const LogicalPoolRepoItem &lr) override {
        return repo::OperationOK;
    }

    int LoadLogicalPoolRepoItems(
        std::vector<LogicalPoolRepoItem> *logicalPoolList) override {
        return repo::OperationOK;
    }

    int DeleteLogicalPoolRepoItem(LogicalPoolIDType id) override {
        return repo::OperationOK;
    }

    int UpdateLogicalPoolRepoItem(const LogicalPoolRepoItem &lr) override {
        return repo::OperationOK;
    }

    int QueryLogicalPoolRepoItem(LogicalPoolIDType id,
                                    LogicalPoolRepoItem *repo) override {
        return repo::OperationOK;
    }

    // copyset operation
    int InsertCopySetRepoItem(const CopySetRepoItem &cr) override {
        return repo::OperationOK;
    }

    int LoadCopySetRepoItems(
        std::vector<CopySetRepoItem> *copySetList) override {
        return repo::OperationOK;
    }

    int DeleteCopySetRepoItem(CopySetIDType id, LogicalPoolIDType lid)
                                                                    override {
        return repo::OperationOK;
    }

    int UpdateCopySetRepoItem(const CopySetRepoItem &cr) override {
        return repo::OperationOK;
    }

    int QueryCopySetRepoItem(CopySetIDType id,
                                LogicalPoolIDType lid,
                                CopySetRepoItem *repo) override {
        return repo::OperationOK;
    }

    // session operation
    int InsertSessionRepoItem(const SessionRepoItem &r) override {
        LOG(INFO) << "InsertSessionRepo";
        return repo::OperationOK;
    }

    int LoadSessionRepoItems(
        std::vector<SessionRepoItem> *sessionList) override {
        LOG(INFO) << "LoadSessionRepo";
        return repo::OperationOK;
    }

    int DeleteSessionRepoItem(const std::string &sessionID) override {
        LOG(INFO) << "DeleteSessionRepo";
        return repo::OperationOK;
    }

    int UpdateSessionRepoItem(const SessionRepoItem &r) override {
        LOG(INFO) << "UpdateSessionRepo";
        return repo::OperationOK;
    }

    int QuerySessionRepoItem(const std::string &sessionID, SessionRepoItem *r) {
        LOG(INFO) << "QuerySessionRepo";
        return repo::OperationOK;
    }
};
}  // namespace mds
}  // namespace curve
#endif  // TEST_MDS_NAMESERVER2_FAKES_H_