void 
copy_memory(void *dest_init, void *src_init, U32 size)
{
	U8 *dest = (U8 *)dest_init;
	U8 *src  = (U8 *)src_init;
	
	for (U32 i = 0; i < size; i++)
	{
		dest[i] = src[i];
	}
}

B32 maxU32(U32 num1, U32 num2)
{
	return num1 > num2 ? num1 :num2;
}

B32 minU32(U32 num1, U32 num2)
{
	return num1 < num2 ? num1 :num2;
}
