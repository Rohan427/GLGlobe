#pragma once

#include <ace/Task.h>
#include <ace/Message_Block.h>
#include <SGP4.h>

namespace Space
{
    struct SatData
    {
        int id;
        libsgp4::Tle tle;
        QVector3D currentPos;
    };

    class SatelliteManager : public ACE_Task<ACE_MT_SYNCH>
    {
        public:
            virtual int svc() override
            {
                while (!this->msg_queue()->is_empty() || !done_)
                {
                    // 1. Loop through all SatData
                    // 2. Run SGP4 FindPosition
                    // 3. Update thread-safe buffer
                    ACE_OS::sleep(ACE_Time_Value (0, 100000)); // 100ms update
                }

                return 0;
            }

        private:
            std::vector<SatData> m_allSats;
            bool done_ = false;
    };
} // namespace Space
