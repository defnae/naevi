// source/naevi/headers/naevi.h

#ifndef NAEVI_H
#define NAEVI_H

typedef struct PieceNode {
	__SIZE_TYPE__ StartOffset;
	__SIZE_TYPE__ Length;
	__SIZE_TYPE__ LineFeeds;
	__SIZE_TYPE__ SubtreeLength;
	__SIZE_TYPE__ SubtreeLineFeeds;

	struct PieceNode* LeftChild;
	struct PieceNode* RightChild;

	__UINT32_TYPE__ Priority;
	__INT32_TYPE__ ReferenceCount;
	__UINT32_TYPE__ Source;

	__UINT32_TYPE__ padding;
} PieceNode;

#define PN PieceNode

typedef struct {
	PN* LeftNode;
	PN* RightNode;
} SplitResult;

#define SP SplitResult

#endif /* NAEVI_H */
