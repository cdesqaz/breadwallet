//
//  BRWallet.h
//  BreadWallet
//
//  Created by Aaron Voisine on 5/12/13.
//  Copyright (c) 2013 Aaron Voisine <voisine@gmail.com>
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import "BRKeySequence.h"
#import "NSData+Bitcoin.h"

#define BRWalletBalanceChangedNotification @"BRWalletBalanceChangedNotification"

typedef struct _BRUTXO {
    UInt256 hash;
    uint32_t n;
} BRUTXO;

#define brutxo_obj(o) [NSValue value:(o).hash.u8 withObjCType:@encode(BRUTXO)]
#define brutxo_data(o) [NSData dataWithBytes:&((struct { uint32_t u[256/32 + 1]; }) {\
    o.hash.u32[0], o.hash.u32[1], o.hash.u32[2], o.hash.u32[3],\
    o.hash.u32[4], o.hash.u32[5], o.hash.u32[6], o.hash.u32[7],\
    CFSwapInt32HostToLittle(o.n) }) length:sizeof(UInt256) + sizeof(uint32_t)]

@class BRTransaction;
@protocol BRKeySequence;

@interface BRWallet : NSObject

@property (nonatomic, readonly) uint64_t balance; // current wallet balance excluding transactions known to be invalid
@property (nonatomic, readonly) NSString *receiveAddress; // returns the first unused external address
@property (nonatomic, readonly) NSString *changeAddress; // returns the first unused internal address
@property (nonatomic, readonly) NSSet *addresses; // all previously generated internal and external addresses
@property (nonatomic, readonly) NSArray *unspentOutputs; // NSValue objects containing UTXO structs
@property (nonatomic, readonly) NSArray *recentTransactions; // BRTransaction objects sorted by date, most recent first
@property (nonatomic, readonly) uint64_t totalSent; // the total amount spent from the wallet (excluding change)
@property (nonatomic, readonly) uint64_t totalReceived; // the total amount received by the wallet (excluding change)
@property (nonatomic, assign) uint64_t feePerKb; // fee per kb of transaction size to use when including tx fee
@property (nonatomic, readonly) uint64_t minOutputAmount; // outputs below this amount are uneconomical due to fees

- (instancetype)initWithContext:(NSManagedObjectContext *)context sequence:(id<BRKeySequence>)sequence
masterPublicKey:(NSData *)masterPublicKey seed:(NSData *(^)(NSString *authprompt, uint64_t amount))seed;

// true if the address is controlled by the wallet
- (BOOL)containsAddress:(NSString *)address;

// true if the address was previously used as an input or output in any wallet transaction
- (BOOL)addressIsUsed:(NSString *)address;

// Wallets are composed of chains of addresses. Each chain is traversed until a gap of a certain number of addresses is
// found that haven't been used in any transactions. This method returns an array of <gapLimit> unused addresses
// following the last used address in the chain. The internal chain is used for change addresses and the external chain
// for receive addresses.
- (NSArray *)addressesWithGapLimit:(NSUInteger)gapLimit internal:(BOOL)internal;

// returns an unsigned transaction that sends the specified amount from the wallet to the given address
- (BRTransaction *)transactionFor:(uint64_t)amount to:(NSString *)address withFee:(BOOL)fee;

// returns an unsigned transaction that sends the specified amounts from the wallet to the specified output scripts
- (BRTransaction *)transactionForAmounts:(NSArray *)amounts toOutputScripts:(NSArray *)scripts withFee:(BOOL)fee;

// sign any inputs in the given transaction that can be signed using private keys from the wallet
- (BOOL)signTransaction:(BRTransaction *)transaction withPrompt:(NSString *)authprompt;

// true if the given transaction is associated with the wallet (even if it hasn't been registered), false otherwise
- (BOOL)containsTransaction:(BRTransaction *)transaction;

// adds a transaction to the wallet, or returns false if it isn't associated with the wallet
- (BOOL)registerTransaction:(BRTransaction *)transaction;

// removes a transaction from the wallet along with any transactions that depend on its outputs
- (void)removeTransaction:(UInt256)txHash;

// returns the transaction with the given hash if it's been registered in the wallet (might also return non-registered)
- (BRTransaction *)transactionForHash:(UInt256)txHash;

// true if no previous wallet transaction spends any of the given transaction's inputs, and no input tx is invalid
- (BOOL)transactionIsValid:(BRTransaction *)transaction;

// returns true if all sequence numbers are final (otherwise transaction can be replaced-by-fee), if no outputs are
// dust, transaction size is not over TX_MAX_SIZE, timestamp is greater than 0, and no inputs are known to be unverfied
- (BOOL)transactionIsVerified:(BRTransaction *)transaction;

// returns true if transaction won't be valid by blockHeight + 1 or within the next 10 minutes
- (BOOL)transactionIsPostdated:(BRTransaction *)transaction atBlockHeight:(uint32_t)blockHeight;

// set the block heights and timestamps for the given transactions, use a height of TX_UNCONFIRMED and timestamp of 0 to
// indicate a transaction and it's dependents should remain marked as unverified (not 0-conf safe)
- (void)setBlockHeight:(int32_t)height andTimestamp:(NSTimeInterval)timestamp forTxHashes:(NSArray *)txHashes;

// returns the amount received by the wallet from the transaction (total outputs to change and/or receive addresses)
- (uint64_t)amountReceivedFromTransaction:(BRTransaction *)transaction;

// retuns the amount sent from the wallet by the trasaction (total wallet outputs consumed, change and fee included)
- (uint64_t)amountSentByTransaction:(BRTransaction *)transaction;

// returns the fee for the given transaction if all its inputs are from wallet transactions, UINT64_MAX otherwise
- (uint64_t)feeForTransaction:(BRTransaction *)transaction;

// historical wallet balance after the given transaction, or current balance if transaction is not registered in wallet
- (uint64_t)balanceAfterTransaction:(BRTransaction *)transaction;

// returns the block height after which the transaction is likely to be processed without including a fee
- (uint32_t)blockHeightUntilFree:(BRTransaction *)transaction;

// fee that will be added for a transaction of the given size in bytes
- (uint64_t)feeForTxSize:(NSUInteger)size;

@end