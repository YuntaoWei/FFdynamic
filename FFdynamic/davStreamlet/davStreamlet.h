#pragma once

#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <mutex>
#include <typeinfo>
#include <typeindex>

#include "davWave.h"
#include "davUtil.h"
#include "davMessager.h"

namespace ff_dynamic  {
using ::std::string;
using ::std::vector;
using ::std::shared_ptr;
using ::std::unique_ptr;

//////////////////////////////////////////////////////////////////////////////////////////
/* options that used to create a streamlet; Right now, only this one */
struct DavOptionBufLimitNum : public DavOption {
    DavOptionBufLimitNum() :
        DavOption(type_index(typeid(*this)), type_index(typeid(int)), "StreamletBufLimitNum") {}
};

using DavStreamletOption = DavDict;

//////////////////////////////////////////////////////////////////////////////////////////
/* Be aware: object slicing here, make sure derived class has no members */
struct DavStreamletTag {
    DavStreamletTag() = default;
    explicit DavStreamletTag(const string & streamletName, const std::type_index & category) {
        setTag(streamletName, category);
    }
    void setTag(const string & streamletName, const std::type_index & category) {
        m_streamletName = streamletName;
        m_streamletCategory = category;
    }
    string dumpTag() const {
        return "[streamletName: " + m_streamletName + ", category: " + m_streamletCategory.name() + "]";
    }

    string m_streamletName;
    std::type_index m_streamletCategory {type_index(typeid(DavStreamletTag))};
};

struct DavUnknownStreamletTag : public DavStreamletTag {
    DavUnknownStreamletTag() : DavStreamletTag("unknownStreamlet", type_index(typeid(DavUnknownStreamletTag))) {}
    explicit DavUnknownStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavUnknownStreamletTag))) {}
};

struct DavDefaultInputStreamletTag : public DavStreamletTag {
    DavDefaultInputStreamletTag() :
        DavStreamletTag("DefaultInputStreamlet", type_index(typeid(DavDefaultInputStreamletTag))) {}
    explicit DavDefaultInputStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavDefaultInputStreamletTag))) {}
};
struct DavDefaultOutputStreamletTag : public DavStreamletTag {
    DavDefaultOutputStreamletTag()
        : DavStreamletTag("DefaultOutputStreamlet", type_index(typeid(DavDefaultOutputStreamletTag))) {}
    explicit DavDefaultOutputStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavDefaultOutputStreamletTag))) {}
};
struct DavMixStreamletTag : public DavStreamletTag {
    DavMixStreamletTag(): DavStreamletTag("MixStreamlet", type_index(typeid(DavMixStreamletTag))) {}
    explicit DavMixStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavMixStreamletTag))) {}
};

/* SingleWave streamlet contain only one DavWave, it is just a wrapper */
struct DavSingleWaveStreamletTag : public DavStreamletTag {
    DavSingleWaveStreamletTag(): DavStreamletTag("SingleWaveStreamlet",
                                                 type_index(typeid(DavSingleWaveStreamletTag))) {}
    explicit DavSingleWaveStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavSingleWaveStreamletTag))) {}
};

extern bool operator<(const DavStreamletTag & l, const DavStreamletTag & r);
extern bool operator==(const DavStreamletTag & l, const DavStreamletTag & r);
extern std::ostream & operator<<(std::ostream & os, const DavStreamletTag & streamletTag);

//////////////////////////////////////////////////////////////////////////////////////////
class DavStreamlet {
public:
    DavStreamlet() {
        m_streamletGroupId = reinterpret_cast<size_t>(this);
        m_streamletTag.setTag(std::to_string(m_streamletGroupId), type_index(typeid(DavUnknownStreamletTag)));
    }
    explicit DavStreamlet (const size_t streamletGroupId) {
        m_streamletGroupId = streamletGroupId;
        m_streamletTag.setTag(std::to_string(m_streamletGroupId), type_index(typeid(DavUnknownStreamletTag)));
    }
    explicit DavStreamlet (const DavStreamletTag & streamletTag) {
        m_streamletGroupId = reinterpret_cast<size_t>(this);
        m_streamletTag = streamletTag;
    }
    DavStreamlet (const size_t streamletGroupId, const DavStreamletTag & streamletTag) {
        m_streamletGroupId = streamletGroupId;
        m_streamletTag = streamletTag;
    }
    virtual ~DavStreamlet() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_davWaves.clear();
        m_audioInEntries.clear();
        m_audioOutEntries.clear();
        m_videoInEntries.clear();
        m_videoOutEntries.clear();
    }

public:
    int start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & w : m_davWaves)
            w->start();
        return 0;
    }
    int pause() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & w : m_davWaves)
            w->pause();
        return 0;
    }
    int resume() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & w : m_davWaves)
            w->resume();
        return 0;
    }
    int stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & w : m_davWaves)
            w->stop();
        return 0;
    }
    bool isStopped() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & w : m_davWaves)
            if (!w->isStopped())
                return false;
        return true;
    }
    int reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & w : m_davWaves)
            w->reset();
        return 0;
    }
    int clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_davWaves.clear();
        m_audioInEntries.clear();
        m_audioOutEntries.clear();
        m_videoInEntries.clear();
        m_videoOutEntries.clear();
        return 0;
    }
    int getErr() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & w : m_davWaves)
            if (w->hasErr())
                return w->getErr().m_msgCode;
        return 0;
    }

public:
    int setWaves(const vector<shared_ptr<DavWave>> & ws) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_davWaves = ws;
        for (auto & w : m_davWaves)
            w->setGroupId(m_streamletGroupId);
        return 0;
    }
    int addOneWave(const shared_ptr<DavWave> & one) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        one->setGroupId(m_streamletGroupId);
        m_davWaves.emplace_back(one);
        return 0;
    }
    vector<shared_ptr<DavWave>> & getWaves() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_davWaves;
    }
    vector<shared_ptr<DavWave>> getWavesByCategory(const DavWaveClassCategory & waveCategory) {
        std::lock_guard<std::mutex> lock(m_mutex);
        vector<shared_ptr<DavWave>> waves;
        for (auto & w : m_davWaves)
            if (w->getDavWaveCategory() == waveCategory)
                waves.emplace_back(w);
        return waves;
    }

    /* connection's use */
    inline vector<shared_ptr<DavWave>> & getAudioInEntries() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_audioInEntries;
    }
    inline vector<shared_ptr<DavWave>> & getAudioOutEntries() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_audioOutEntries;
    }
    inline vector<shared_ptr<DavWave>> & getVideoInEntries() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_videoInEntries;
    }
    inline vector<shared_ptr<DavWave>> & getVideoOutEntries() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_videoOutEntries;
    }

    /* set all */
    inline void setAudioInEntries(const vector<shared_ptr<DavWave>> & waves) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_audioInEntries = waves;
    }
    inline void setVideoInEntries(const vector<shared_ptr<DavWave>> & waves) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_videoInEntries = waves;
    }
    inline void setAudioOutEntries(const vector<shared_ptr<DavWave>> & waves) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_audioOutEntries = waves;
    }
    inline void setVideoOutEntries(const vector<shared_ptr<DavWave>> & waves) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_videoOutEntries = waves;
    }
    /* add one */
    inline void addOneAudioInEntry(const shared_ptr<DavWave> & wave) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_audioInEntries.emplace_back(wave);
    }
    inline void addOneVideoInEntry(const shared_ptr<DavWave> & wave) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_videoInEntries.emplace_back(wave);
    }
    inline void addOneAudioOutEntry(const shared_ptr<DavWave> & wave) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_audioOutEntries.emplace_back(wave);
    }
    inline void addOneVideoOutEntry(const shared_ptr<DavWave> & wave) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_videoOutEntries.emplace_back(wave);
    }

public:
    /* trivial helpers */
    inline void setGroupId(size_t streamletGroupId) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_streamletGroupId = streamletGroupId;
        for (auto & w : m_davWaves)
            w->setGroupId(m_streamletGroupId);
    }
    inline size_t getGroupId() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_streamletGroupId;
    }
    inline void setTag(const DavStreamletTag & tag) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_streamletTag = tag;
    }
    inline const DavStreamletTag & getTag() const noexcept {
        return m_streamletTag;
    }
    /* not used right now */
    inline void lock() {m_mutex.lock();}
    inline void unlock() {m_mutex.unlock();}

private:
    std::mutex m_mutex;
    DavStreamletTag m_streamletTag;
    size_t m_streamletGroupId; /* used for waves that in the same streamlet */
    vector<shared_ptr<DavWave>> m_davWaves;
    /* TODO: distince video/audio raw or bitstream In/Out Entry */
    vector<shared_ptr<DavWave>> m_audioInEntries;
    vector<shared_ptr<DavWave>> m_audioOutEntries;
    vector<shared_ptr<DavWave>> m_videoInEntries;
    vector<shared_ptr<DavWave>> m_videoOutEntries;
};

extern DavStreamlet &  operator>>(DavStreamlet & dst, DavStreamlet & src);
extern shared_ptr<DavStreamlet> & operator>>(shared_ptr<DavStreamlet> & dst,
                                             shared_ptr<DavStreamlet> & src);

/* a simple wrapper for a set of streamlet */
class DavRiver {
public:
    DavRiver() = default;
    ~DavRiver() = default;
    explicit DavRiver(const vector<shared_ptr<DavStreamlet>> & streamlets) {
        init(streamlets);
    }
    int init(const vector<shared_ptr<DavStreamlet>> & streamlets) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & s : streamlets)
            m_river.emplace(s->getTag(), s);
        return 0;
    }
    int add(const shared_ptr<DavStreamlet> & streamlet) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_river.emplace(streamlet->getTag(), streamlet);
        return 0;
    }
    shared_ptr<DavStreamlet> get(const DavStreamletTag & streamletTag) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_river.count(streamletTag))
            return {};
        return m_river.at(streamletTag);
    }
    vector<shared_ptr<DavStreamlet>> getStreamlets() {
        std::lock_guard<std::mutex> lock(m_mutex);
        vector<shared_ptr<DavStreamlet>> streamlets;
        for (auto & r : m_river)
            streamlets.emplace_back(r.second);
        return streamlets;
    }
    vector<shared_ptr<DavStreamlet>> getStreamletsByCategory(const DavStreamletTag & tag) {
        std::lock_guard<std::mutex> lock(m_mutex);
        vector<shared_ptr<DavStreamlet>> streamlets;
        for (auto & r : m_river)
            if (r.first.m_streamletCategory == tag.m_streamletCategory)
                streamlets.emplace_back(r.second);
        return streamlets;
    }
    size_t count(const DavStreamletTag & streamletTag) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_river.count(streamletTag);
    }
    int erase(const DavStreamletTag & streamletTag) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_river.erase(streamletTag);
        return 0;
    }
    int clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_river.clear();
        return 0;
    }

public:
    void start() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & s : m_river) {
            LOG(INFO) << s.first << " started";
            s.second->start();
        }
    }
    void stop() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & s : m_river)
            s.second->stop();
    }
    bool isStopped() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & s : m_river)
            if (!s.second->isStopped())
                return false;
        return true;
    }

public:
    string dumpRiver() {
        std::lock_guard<std::mutex> lock(m_mutex);
        string dumpStr;
        for (auto & s : m_river) {
            dumpStr += s.first.dumpTag();
            dumpStr += "\n";
        }
        return dumpStr;
    }
    int getErr() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & s : m_river) {
            int ret = s.second->getErr();
            if (ret < 0)
                return ret;
        }
        return 0;
    }

private:
    std::mutex m_mutex;
    // key is the m_streamletName
    map<DavStreamletTag, shared_ptr<DavStreamlet>> m_river;
};

} // namespace ff_dynamic
