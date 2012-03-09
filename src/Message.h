//
//  Error.h
//  Turing Compiler
//
//  Created by Tristan Hume on 12-02-01.
//  Copyright 2012 15 Norwich Way. All rights reserved.
//

#ifndef Turing_Compiler_Error__H
#define Turing_Compiler_Error__H

#include <string>
#include <llvm/ADT/Twine.h>

namespace Message {
    //! sets the line number to be used for subsequent error messages.
    //! set to 0 for no line number.
    void setCurLine(int line,std::string fileName);
    
    //! returns false on fatality (false for Error true for others)
    bool error(const llvm::Twine &message, bool warning = false);
    bool log(const llvm::Twine &message);
    bool ifNull(void *cond, const llvm::Twine &message);
    
    void runtimeError(const llvm::Twine &message);
    
    struct Exception {
        Exception(){
            // TODO make it take on the message of the last Message::error
            Message = "";
        }
        Exception(const llvm::Twine &message) {
            Message = message.str();
        }
        std::string Message;
    };
}

#endif
