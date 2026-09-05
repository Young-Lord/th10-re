#include <stdio.h>
#include <string.h>

#include "AsciiRenderAdapter.hpp"

namespace th10 {

void QueuePrimaryFpsTextSelected(AsciiManagerAdapterSlice *manager,
                                 u32 color, float sampled_fps)
{
    manager->color = color;

    char text[0x200];
    sprintf(text, "%2.1ffps", static_cast<double>(sampled_fps));

    if (manager->primary_count < 256) {
        AsciiTextEntry *entry = manager->primary_entries + manager->primary_count;
        strcpy(entry->text, text);
        entry->position.x = 590.0f;
        entry->position.y = 470.0f;
        entry->position.z = 0.0f;
        entry->color = manager->color;
        entry->scale_x = manager->scale_x;
        entry->scale_y = manager->scale_y;
        entry->gui_mode = manager->gui_mode;
        entry->selected = 0;
        entry->text_mode = manager->text_mode;
        manager->primary_count++;
    }

    // 0x00401690 applies this store even if insertion was skipped at capacity.
    manager->primary_entries[manager->primary_count - 1].selected = 1;
    manager->color = 0xffffffff;
}

} // namespace th10
