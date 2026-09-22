# GrandMobile-VM
reversed virtual machine who used by CR:MP mobile project "Grand Mobile". 


# VM Description 

this is stack vm with two stacks:
  opstack — 16 kib, grows up by 8 bytes per push
  locals  — 8 kib memory, base is on offset 4096

vm parameters (passed on init):
  a1 (int64) — pointer to data buffer
  a2 (int32) — buffer length
  a3 (int32) — seed (can be changed by bytecode)

notation which i use below:
  top32   — top i32 of stack (at sp)
  top64   — top i64 of stack
  below32 — i32 which lies 8 bytes below top
  below64 — i64 which lies 8 bytes below top
  imm32/imm64 — immediate operand (little-endian) in bytecode stream, right after opcode byte

attention: for store-ops (0x9c, 0xd3) pointer is on top and value below.
only store_i64 (0x1b) is opposite — pointer in below64, value in top64. very tricky moment,
i lost 2 hours on it while was reversing.

```text
------------------------------------------------------------
opcode | mnemonic            | operand | what it do
------------------------------------------------------------
```
0x0c   | u8_trunc_i32        |    -    | top32 = (uint8_t)top32
0x0e   | sub_i64             |    -    | below64 = below64 - top64; then pop 8
0x11   | u8_mask_i32         |    -    | top32 = (uint8_t)top32  (same as 0x0c, dublicate)
0x1b   | store_i64           |    -    | *(int64_t*)below64 = top64; pop 16
0x1d   | load_i64            |    -    | top64 = *(int64_t*)top64
0x21   | push_i32_const      |  imm32  | push i64(0), then top32 = imm32   (same like 0xdf)
0x25   | add_i64             |    -    | below64 = below64 + top64; pop 8
0x31   | xor_i32             |    -    | below32 = below32 xor top32; pop 8   (same like 0xd1)
0x34   | load_i32            |    -    | top32 = *(int32_t*)top64
0x3a   | umod_i32            |    -    | below32 = (u32)below32 mod (u32)top32; pop 8
       |                     |         |   throws exception if divisor is zero
0x43   | load_u8             |    -    | top32 = *(uint8_t*)top64  (zero-extend byte to i32)
0x46   | add_i32             |    -    | below32 = below32 + top32; pop 8
0x4a   | jmp_if_nz           |  imm32  | cond = top32; pop 8;
       |                     |         |   if (cond != 0) then ip += imm32, otherwise ip += 4
0x53   | or_i32              |    -    | below32 = below32 or top32; pop 8
0x58   | udiv_i32_swapped    |    -    | below32 = (u32)top32 / (u32)below32; pop 8
       |                     |         |   nb! operands are reversed, i dont know why
       |                     |         |   author of vm done it in such way
0x67   | ret                 |    -    | halt vm and return from run()
0x6c   | nop                 |    -    | does nothing                  (same like 0xe3)
0x71   | zext_i32_to_i64     |    -    | top64 = (uint64_t)(uint32_t)top32
0x7a   | mul_i64             |    -    | below64 = below64 * top64; pop 8
0x84   | lea_local           |  imm32  | push &locals[4096 + imm32]
       |                     |         |   (basically: get address of local variable)
0x8c   | u8_trunc            |    -    | top32 = (uint8_t)top32        (again same as 0x0c)
0x94   | ult_i64             |    -    | below32 = ((u64)below64 < (u64)top64) ? 1 : 0; pop 8
0x98   | mul_i32             |    -    | below32 = below32 * top32; pop 8
0x9c   | store_u8            |    -    | *(uint8_t*)top64 = (uint8_t)below32; pop 16
0xa4   | jmp                 |  imm32  | ip += imm32  (unconditional relative jump)
0xad   | sext_i32_to_i64     |    -    | top64 = (int64_t)(int32_t)top32  (sign extension)
0xae   | ult_i32_swapped     |    -    | below32 = ((u32)top32 < (u32)below32) ? 1 : 0; pop 8
       |                     |         |   again with reversed operands, like 0x58
0xbc   | lea_param           |  imm32  | push address of vm param:
       |                     |         |    imm=0 -> &a1 (buf pointer)
       |                     |         |    imm=1 -> &a2 (buf length)
       |                     |         |    imm=2 -> &a3 (seed)
0xc3   | shl_i32             |    -    | below32 = (u32)below32 << (top32 and 31); pop 8
0xc6   | and_i32             |    -    | below32 = below32 and top32; pop 8
0xd1   | xor_i32             |    -    | (same like 0x31)
0xd3   | store_i32           |    -    | *(int32_t*)top64 = below32; pop 16
0xdf   | push_i32_const      |  imm32  | (same like 0x21)
0xe3   | nop                 |    -    | (same like 0x6c)
0xf1   | push_i64_const      |  imm64  | push imm64

```text
------------------------------------------------------------
some observations about how this vm works:

1) push always increments sp by 8, even when we push i32. small values are stored
   in low half of 8-byte slot, high bits are just garbage or zero.

2) store operations (0x1b / 0x9c / 0xd3) take pair (value, pointer) from stack:
   * for 0x9c and 0xd3 pointer is on top, value is below
   * for 0x1b its opposite (pointer below, value on top)
   this inconsistency looks like bug in original vm but algoritm relies on it
   so we must emulate exactly.

3) 0x21 and 0xdf do same thing but have different opcode. probably compiler
   emitted them from different context. same story with 0x31/0xd1, 0x6c/0xe3, 0x0c/0x8c/0x11.

4) 0x58 (division) and 0xae (comparison) both have swapped operands. its consistent
   inside vm but very confusing when you first look on trace.


idioms that we can see in this concrete bytecode:

  df ii ii ii ii                     — push 32-bit constant
  84 oo oo oo oo                     — take address of local[offs]
  84 oo oo oo oo 34                  — load i32 from local
  84 oo oo oo oo <expr> d3           — store i32 into local
  bc 02 00 00 00 34                  — load seed (a3) to stack
  bc 00 00 00 00 34                  — load low 32 bits of buffer pointer (a1)
  71 f1 01 00 00 00 00 00 00 00 00 7a
                                     — zext to i64, push 1, multiply. looks like weird way
                                       to just "extend to i64", author probably was lazy
                                       to add proper zext-only opcode
  a4 dd dd dd dd                     — unconditional jump by displacement dd
  4a dd dd dd dd                     — conditional jump if top is not zero

if you want understand full algoritm of encryption — look on encrypt_native_raw() in vm.cpp,
its already reversed from this bytecode and works 1-to-1 with vm.
```
