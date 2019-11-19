//
//  helper.c
//  backTCP
//
//  Created by bbn on 2019/11/18.
//  Copyright © 2019 bbn. All rights reserved.
//

#include "helper.h"

/**
 * panic: panic with facing errors.
 * @param msg panic message
 */
void kpanic(char* msg) {
    fprintf(stderr, "ERROR %s\n", msg);
    exit(1);
}
