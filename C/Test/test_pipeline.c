#include <stdio.h>
#include <stdlib.h>
#include "pipeline.h"

int main(void) {
    printf("=== Pipeline Test ===\n\n");
    
    const char* files[] = {
        "../Example/Nat.pind",
        "../Example/List.pind",
        "../Example/Tree.pind",
        "../Example/Sort.pind",
        "../Example/FirstOrderlogic.pind",
        "../Example/FirstOrderlogic_advanced.pind",
        "../Example/PropositionLogic.pind",
        "../Example/ExistsNested.pind",
    };
    int num_files = sizeof(files) / sizeof(files[0]);
    
    int pass_count = 0;
    int fail_count = 0;
    
    for (int f = 0; f < num_files; f++) {
        printf("Test: %s\n", files[f]);
        PipelineResult result;
        
        if (check_file(files[f], CONV_WHN, &result)) {
            printf("  OK (%u globals)\n", result.global_ctx.global_count);
            pass_count++;
        } else {
            printf("  FAIL: %s\n", result.error_msg);
            fail_count++;
        }
        pipeline_result_free(&result);
    }
    
    printf("\n=== %d/%d passed ===\n", pass_count, num_files);
    return fail_count > 0 ? 1 : 0;
}
