#pragma once

class IRunnable
{
public:
    IRunnable() : m_isRunning(false)
    {
    }

    virtual ~IRunnable() = default;

    bool IsRunning() const
    {
        return m_isRunning;
    }

    virtual bool Run() = 0;

protected:
    bool m_isRunning;
};
