#pragma once

#include "anim/keyframe.hpp"
#include "core/handle.hpp"
#include "core/manager.hpp"
#include "core/singlton.hpp"
#include "misc/timer.hpp"

namespace nickel {

class AnimationTrack {
public:
    AnimationTrack() = default;

    AnimationTrack(const mirrow::drefl::type* typeInfo)
        : typeInfo_(typeInfo) {}

    virtual ~AnimationTrack() = default;

    virtual mirrow::drefl::any GetValueAt(TimeType) const = 0;
    virtual bool Empty() const = 0;
    virtual size_t Size() const = 0;
    virtual TimeType Duration() const = 0;

    auto TypeInfo() const { return typeInfo_; }

    void ChangeApplyTarget(const mirrow::drefl::type* typeInfo) { applyTypeInfo_ = typeInfo; }
    auto GetApplyTarget() const { return applyTypeInfo_; }

private:
    const mirrow::drefl::type*
        applyTypeInfo_;  // which type should this track apply to
    const mirrow::drefl::type* typeInfo_;  // animate data type
};

template <typename T>
class BasicAnimationTrack final : public AnimationTrack {
public:
    using keyframe_type = KeyFrame<T>;
    using container_type = std::vector<keyframe_type>;
    using time_type = typename keyframe_type::time_type;

    BasicAnimationTrack()
        : AnimationTrack(mirrow::drefl::typeinfo<T>()) {}

    BasicAnimationTrack(const container_type& keyPoints)
        : AnimationTrack(mirrow::drefl::typeinfo<
                         typename keyframe_type::value_type>()),
          keyPoints_(keyPoints) {}

    BasicAnimationTrack(container_type&& keyPoints)
        : AnimationTrack(mirrow::drefl::typeinfo<
                         typename keyframe_type::value_type>()),
          keyPoints_(std::move(keyPoints)) {}

    bool Empty() const override { return keyPoints_.empty(); }

    size_t Size() const override { return keyPoints_.size(); }

    TimeType Duration() const override {
        return keyPoints_.empty() ? 0 : keyPoints_.back().timePoint;
    }

    auto& KeyPoints() const { return keyPoints_; }

    mirrow::drefl::any GetValueAt(TimeType t) const override {
        if (Empty()) {
            return {};
        }

        if (Size() == 1) {
            return mirrow::drefl::any_make_ref(keyPoints_[0].value);
        }

        auto& last = keyPoints_.back();
        if (t >= last.timePoint) {
            return mirrow::drefl::any_make_ref(last.value);
        }

        for (int i = 0; i < keyPoints_.size() - 1; i++) {
            auto& begin = keyPoints_[i];
            auto& end = keyPoints_[i + 1];

            if (begin.timePoint <= t && t < end.timePoint) {
                return mirrow::drefl::any_make_copy(begin.interpolate(
                    static_cast<float>(t) / (end.timePoint - begin.timePoint),
                    begin.value, end.value));
            }
        }

        return {};
    }

private:
    container_type keyPoints_;
};

class AnimTrackSerialMethods : public Singlton<AnimTrackSerialMethods, false> {
public:
    using serialize_fn_type = toml::table (*)(const AnimationTrack&);
    using deserialize_fn_type =
        std::unique_ptr<AnimationTrack> (*)(const toml::table&);
    using type_info_type = const mirrow::drefl::type*;

    bool Contain(type_info_type type);
    serialize_fn_type GetSerializeMethod(type_info_type type);
    deserialize_fn_type GetDeserializeMethod(type_info_type);

    template <typename T>
    auto& RegistMethod() {
        auto type_info = mirrow::drefl::typeinfo<T>();
        methods_.emplace(type_info,
                         std::make_pair(serialize<T>, deserialize<T>));
        return *this;
    }

private:
    std::unordered_map<type_info_type,
                       std::pair<serialize_fn_type, deserialize_fn_type>>
        methods_;

    template <typename T>
    static toml::table serialize(const AnimationTrack& animTrack) {
        auto& track = static_cast<const BasicAnimationTrack<T>&>(animTrack);
        toml::table tbl;

        toml::array keyframeTbl;
        mirrow::serd::srefl::serialize(track.KeyPoints(), keyframeTbl);
        tbl.emplace("keyframe", keyframeTbl);
        auto type_info = mirrow::drefl::typeinfo<T>();

        if (animTrack.GetApplyTarget()) {
            tbl.emplace("apply_target", animTrack.GetApplyTarget()->name());
        }
        tbl.emplace("type", type_info->name());

        return tbl;
    }

    template <typename T>
    static std::unique_ptr<AnimationTrack> deserialize(const toml::table& tbl) {
        auto type_info = mirrow::drefl::typeinfo<T>();
        auto typeNode = tbl["type"];

        Assert(typeNode.is_string(), "type is not string");
        auto typeStr = typeNode.as_string()->get();
        Assert(typeStr == type_info->name(), "deserialize table type not fit");

        auto keyframes = tbl["keyframe"];
        Assert(keyframes.is_array(),
               "deserialize KeyPoint must has toml::array node");

        typename BasicAnimationTrack<T>::container_type track;
        mirrow::serd::srefl::deserialize(*keyframes.as_array(), track);

        auto animTrack = std::make_unique<BasicAnimationTrack<T>>(std::move(track));

        if (auto node = tbl["apply_target"]; node.is_string()) {
            animTrack->ChangeApplyTarget(::mirrow::drefl::typeinfo(node.as_string()->get()));
        }

        return animTrack;
    }
};

class Animation final : public Asset {
public:
    using track_base_type = AnimationTrack;
    using track_pointer_type = std::unique_ptr<track_base_type>;
    using container_type = std::unordered_map<std::string, track_pointer_type>;

    static Animation Null;

    Animation() = default;

    Animation(container_type&& tracks) : tracks_(std::move(tracks)) {
        lastTime_ = 0;
        for (auto& [name, track] : tracks_) {
            auto duration = track->Duration();
            lastTime_ = std::max<TimeType>(lastTime_, duration);
        }
    }

    auto& Tracks() const { return tracks_; }

    TimeType Duration() const { return lastTime_; }

private:
    container_type tracks_;
    TimeType lastTime_;
};

using AnimationHandle = Handle<Animation>;

class AnimationManager final : public Manager<Animation> {
public:
    AnimationHandle CreateFromTracks(
        typename Animation::container_type&& tracks);
    std::shared_ptr<Animation> CreateSolitaryFromTracks(
        typename Animation::container_type&& tracks);


    toml::table Save2Toml() const override {
        // TODO: implement
        return {};
    }

    void LoadFromToml(toml::table&) override {
        // TODO: implement
    }
};

class AnimationPlayer final {
public:
    enum class Direction {
        Forward = 1,
        Backward = -1,
    };

    using animation_type = Animation;

    AnimationPlayer(AnimationManager& mgr)
        : mgr_(&mgr) {}

    AnimationHandle Anim() const { return handle_; }

    void SetDir(Direction dir) { dir_ = dir; }

    Direction GetDir() const { return dir_; }

    void ChangeAnim(AnimationHandle anim) {
        handle_ = anim;
        Reset();
    }

    void Step(TimeType step) {
        if (isPlaying_) {
            curTime_ += static_cast<int>(dir_) * step;
        }
    }

    void Play() { isPlaying_ = true; }

    bool IsPlaying() const { return isPlaying_; }

    void Stop() { isPlaying_ = false; }

    void SetTick(TimeType t) {
        curTime_ = std::clamp<int>(curTime_, 0, static_cast<int>(Duration()));
    }
    
    bool Empty() const {
        return !handle_;
    }

    AnimationHandle GetAnim() const {
        return handle_;
    }

    void Reset() {
        if (dir_ == Direction::Forward) {
            curTime_ = 0;
        } else {
            curTime_ = static_cast<int>(Duration());
        }
    }

    bool IsValid() const { return mgr_->Has(handle_); }

    TimeType Duration() const {
        if (mgr_->Has(handle_)) {
            auto& anim = mgr_->Get(handle_);
            return anim.Duration();
        }
        return 0;
    }

    void Sync(gecs::entity, gecs::registry);

private:
    AnimationManager* mgr_;
    Direction dir_ = Direction::Forward;
    int curTime_ = 0;
    AnimationHandle handle_;
    bool isPlaying_ = false;
};

}  // namespace nickel