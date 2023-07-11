#pragma once

class IInitializable
{
public:
    IInitializable() : m_isInitialized(false)
    {
    }

    virtual ~IInitializable() = default;

    bool IsInitialized() const
    {
        return m_isInitialized;
    }

protected:
    virtual bool Initialize() = 0;
    virtual void Destroy() = 0;

    bool m_isInitialized;
};
