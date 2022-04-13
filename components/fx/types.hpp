#ifndef OPENMW_COMPONENTS_FX_TYPES_H
#define OPENMW_COMPONENTS_FX_TYPES_H

#include <optional>

#include <osg/Camera>
#include <osg/Uniform>
#include <osg/Texture2D>
#include <osg/FrameBufferObject>
#include <osg/BlendFunc>
#include <osg/BlendEquation>

#include <MyGUI_Widget.h>

#include <components/sceneutil/depth.hpp>
#include <components/settings/shadermanager.hpp>
#include <components/misc/stringops.hpp>
#include <components/debug/debuglog.hpp>

#include "pass.hpp"

namespace fx
{
    namespace Types
    {
        struct SizeProxy
        {
            std::optional<float> mWidthRatio;
            std::optional<float> mHeightRatio;
            std::optional<int> mWidth;
            std::optional<int> mHeight;

            std::tuple<int,int> get(int width, int height) const
            {
                int scaledWidth = width;
                int scaledHeight = height;

                if (mWidthRatio)
                    scaledWidth = width * mWidthRatio.value();
                else if (mWidth)
                    scaledWidth = mWidth.value();

                if (mHeightRatio > 0.f)
                    scaledHeight = height * mHeightRatio.value();
                else if (mHeight)
                    scaledHeight = mHeight.value();

                return std::make_tuple(scaledWidth, scaledHeight);
            }
        };

        struct RenderTarget
        {
            osg::ref_ptr<osg::Texture2D> mTarget = new osg::Texture2D;
            SizeProxy mSize;
            int mMipmapLevels = 0;
        };

        template <class T>
        struct Uniform
        {
            std::optional<T> mValue;
            T mDefault;
            T mMin = std::numeric_limits<T>::lowest();
            T mMax = std::numeric_limits<T>::max();

            using value_type = T;

            T getValue() const
            {
                return mValue.value_or(mDefault);
            }
        };

        using Uniform_t = std::variant<
            Uniform<osg::Vec2f>,
            Uniform<osg::Vec3f>,
            Uniform<osg::Vec4f>,
            Uniform<bool>,
            Uniform<float>,
            Uniform<int>
        >;

        enum SamplerType
        {
            Texture_1D,
            Texture_2D,
            Texture_3D
        };

        struct UniformBase
        {
            std::string mName;
            std::string mHeader;
            std::string mTechniqueName;
            std::string mDescription;

            bool mStatic = true;
            std::optional<SamplerType> mSamplerType = std::nullopt;
            double mStep;

            Uniform_t mData;

            template <class T>
            T getValue() const
            {
                auto value = Settings::ShaderManager::getValue<T>(mTechniqueName, mName);

                return value.value_or(std::get<Uniform<T>>(mData).getValue());
            }

            template <class T>
            T getMin() const
            {
                return std::get<Uniform<T>>(mData).mMin;
            }

            template <class T>
            T getMax() const
            {
                return std::get<Uniform<T>>(mData).mMax;
            }

            template <class T>
            T getDefault() const
            {
                return std::get<Uniform<T>>(mData).mDefault;
            }

            template <class T>
            void setValue(const T& value)
            {
                std::visit([&, value](auto&& arg){
                    using U = typename std::decay_t<decltype(arg)>::value_type;

                    if constexpr (std::is_same_v<T, U>)
                    {
                        arg.mValue = value;

                        if (mStatic)
                            Settings::ShaderManager::setValue<T>(mTechniqueName, mName, value);
                    }
                    else
                    {
                        Log(Debug::Warning) << "Attempting to set uniform '" << mName << "' with wrong type";
                    }
                }, mData);
            }

            void setUniform(osg::Uniform* uniform)
            {
                auto type = getType();
                if (!type || type.value() != uniform->getType())
                    return;

                std::visit([&](auto&& arg)
                {
                    const auto value = arg.getValue();
                    uniform->set(value);
                }, mData);
            }

            std::optional<osg::Uniform::Type> getType() const
            {
                return std::visit([](auto&& arg) -> std::optional<osg::Uniform::Type> {
                    using T = typename std::decay_t<decltype(arg)>::value_type;

                    if constexpr (std::is_same_v<T, osg::Vec2f>)
                        return osg::Uniform::FLOAT_VEC2;
                    else if constexpr (std::is_same_v<T, osg::Vec3f>)
                        return osg::Uniform::FLOAT_VEC3;
                    else if constexpr (std::is_same_v<T, osg::Vec4f>)
                        return osg::Uniform::FLOAT_VEC4;
                    else if constexpr (std::is_same_v<T, float>)
                        return osg::Uniform::FLOAT;
                    else if constexpr (std::is_same_v<T, int>)
                        return osg::Uniform::INT;
                    else if constexpr (std::is_same_v<T, bool>)
                        return osg::Uniform::BOOL;

                    return std::nullopt;
                }, mData);
            }

            std::optional<std::string> getGLSL()
            {
                if (mSamplerType)
                {
                    switch (mSamplerType.value())
                    {
                        case Texture_1D:
                            return Misc::StringUtils::format("uniform sampler1D %s;", mName);
                        case Texture_2D:
                            return Misc::StringUtils::format("uniform sampler2D %s;", mName);
                        case Texture_3D:
                            return Misc::StringUtils::format("uniform sampler3D %s;", mName);
                    }
                }

                bool useUniform = (Settings::ShaderManager::getMode() == Settings::ShaderManager::Mode::Debug || mStatic == false);

                return std::visit([&](auto&& arg) -> std::optional<std::string> {
                    using T = typename std::decay_t<decltype(arg)>::value_type;

                    auto value = arg.getValue();

                    if constexpr (std::is_same_v<T, osg::Vec2f>)
                    {
                        if (useUniform)
                            return Misc::StringUtils::format("uniform vec2 %s;", mName);

                        return Misc::StringUtils::format("const vec2 %s=vec2(%f,%f);", mName, value[0], value[1]);
                    }
                    else if constexpr (std::is_same_v<T, osg::Vec3f>)
                    {
                        if (useUniform)
                            return Misc::StringUtils::format("uniform vec3 %s;", mName);

                        return Misc::StringUtils::format("const vec3 %s=vec3(%f,%f,%f);", mName, value[0], value[1], value[2]);
                    }
                    else if constexpr (std::is_same_v<T, osg::Vec4f>)
                    {
                        if (useUniform)
                            return Misc::StringUtils::format("uniform vec4 %s;", mName);

                        return Misc::StringUtils::format("const vec4 %s=vec4(%f,%f,%f,%f);", mName, value[0], value[1], value[2], value[3]);
                    }
                    else if constexpr (std::is_same_v<T, float>)
                    {
                        if (useUniform)
                            return Misc::StringUtils::format("uniform float %s;",  mName);

                        return Misc::StringUtils::format("const float %s=%f;", mName, value);
                    }
                    else if constexpr (std::is_same_v<T, int>)
                    {
                        if (useUniform)
                            return Misc::StringUtils::format("uniform int %s;",  mName);

                        return Misc::StringUtils::format("const int %s=%i;", mName, value);
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        if (useUniform)
                            return Misc::StringUtils::format("uniform bool %s;", mName);

                        return Misc::StringUtils::format("const bool %s=%s;", mName, value ? "true" : "false");
                    }

                    return std::nullopt;

                }, mData);
            }

        };
    }
}

#endif
