#include "Job.h"

using namespace onenewarm;

TLSObjectPool<onenewarm::Job> s_JobPool(0, false); // JobÀÇ ObjectPool

Job* onenewarm::Job::Alloc()
{
    Job* ret = s_JobPool.Alloc();
    ret->m_Buffer.Clear();
    return ret;
}

void onenewarm::Job::Free(Job* job)
{
    s_JobPool.Free(job);
}

onenewarm::Job::Job()
{
}
