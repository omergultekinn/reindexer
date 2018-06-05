#pragma once

#include "core/idset.h"
#include "core/keyvalue/keyvalue.h"
#include "core/lrucache.h"
#include "core/nsselecter/nsselecter.h"
#include "core/query/query.h"
#include "tools/serializer.h"
#include "vendor/murmurhash/MurmurHash3.h"
namespace reindexer {

struct JoinCacheKey {
	JoinCacheKey() {}
	void SetData(SortType sortId, const Query &q) {
		WrSerializer ser;
		q.Serialize(ser, (SkipJoinQueries | SkipMergeQueries));
		ser.PutVarint(sortId);
		buf_.reserve(buf_.size() + ser.Len());
		buf_.insert(buf_.end(), ser.Buf(), ser.Buf() + ser.Len());
	}
	JoinCacheKey(SortType sortId, const Query &q) { SetData(sortId, q); }
	JoinCacheKey(const JoinCacheKey &other) { buf_ = other.buf_; }
	h_vector<uint8_t, 256> buf_;
};
struct equal_join_cache_key {
	bool operator()(const JoinCacheKey &lhs, const JoinCacheKey &rhs) const {
		return (lhs.buf_.size() == rhs.buf_.size() && memcmp(lhs.buf_.data(), rhs.buf_.data(), lhs.buf_.size()) == 0);
	}
};
struct hash_join_cache_key {
	size_t operator()(const JoinCacheKey &cache) const {
		uint64_t hash[2];
		MurmurHash3_x64_128(cache.buf_.data(), cache.buf_.size(), 0, &hash);
		return hash[0];
	}
};

struct JoinCacheFinalVal {
	size_t Size() const { return ids_ ? ids_->size() * sizeof(IdSet::value_type) : 0; }
	IdSet::Ptr ids_;
	bool matchedAtLeastOnce;
};

typedef LRUCache<JoinCacheKey, JoinCacheFinalVal, hash_join_cache_key, equal_join_cache_key> MainLruCache;

class JoinCacheFinal : public MainLruCache {
public:
	JoinCacheFinal() : MainLruCache(kDefaultCacheSizeLimit / 10, 2) {}
	void SetHitCount(size_t hit) { this->hitCountToCache_ = hit; }
	typedef shared_ptr<JoinCacheFinal> Ptr;
};  // namespace reindexer

struct JoinCacheVal {
	JoinCacheVal() : cache_final_(std::make_shared<JoinCacheFinal>()) {}
	size_t Size() const { return preResult ? preResult->ids.size() * sizeof(IdSet::value_type) : 0; }
	JoinCacheFinal::Ptr cache_final_;
	bool inited = false;
	SelectCtx::PreResult::Ptr preResult;
};

class JoinCache : public LRUCache<JoinCacheKey, JoinCacheVal, hash_join_cache_key, equal_join_cache_key> {
public:
	typedef shared_ptr<JoinCache> Ptr;
};

struct JoinCacheRes {
	bool haveData = false;
	bool needPut = false;

	JoinCacheKey key;
	JoinCache::Iterator it;
};

}  // namespace reindexer
