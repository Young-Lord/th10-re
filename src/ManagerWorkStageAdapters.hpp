#pragma once

#include "ManagerWork.hpp"

namespace th10 {

i32 SetupEncodedManagerWorkStage(WorkRecordPartial *record,
                                 ManagerWorkChainNodePartial *node);
i32 SetupRawManagerWorkStage(WorkRecordPartial *record,
                             ManagerWorkChainNodePartial *node);
void SetupEmptyManagerWorkStage(WorkRecordPartial *record,
                                ManagerWorkChainNodePartial *node);

void CallManagerWorkVirtualSlot1c(void *object, i32 argument);
void CallManagerWorkVirtualSlot24(void *object);
void FillManagerWorkVirtualOutput(void *object,
                                  ManagerWorkStageOutputRecord *output);

} // namespace th10
