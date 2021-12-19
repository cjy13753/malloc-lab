# Dynamic Memory Allocator in my words
아래 정리를 읽기 전 미래의 내가 참고해야할 사항

1. 정글 Week 06 동안 내가 소화한 만큼의 내용만 아래에 정리되어 있다. 따라서 아래에서 자세한 내용을 기대하면 안 된다. 보다 자세한 내용은 당연하게도 CSAPP 3rd 9.9 단원 및 [CMU대학의 저자직강](https://scs.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=d69a8072-3d23-4604-8081-0edeba33bb52)을 참고하도록 한다.

1. 내가 소화한 만큼의 내용이 정리되어 있기 때문에 틀린 내용이 포함되어 있을 수 있다. 복습할 때 틀린 부분을 보게 되면 수정하고 새로운 내용으로 업데이트 할 수 있도록 한다.

1. Explicit free list를 제외하고 Implict free list와 Segregated free list에 대한 내용만 정리되어 있다. 

## Dynamic memory allocator는 어떤 역할을 하길래 중요한가?
Application이 필요로 하는 데이터의 크기를 runtime 전까지 알 수 없을 때, 만약 static하게 (1)미리 말도 안 되게 크게 memory를 할당해주거나 (2)느낌상 적당하게 memory를 할당해준다면? (1)의 문제는 internal fragmentation(이 개념은 밑에서 더 자세히)의 정도가 심화되어 memory utilization이 크게 떨어진다. 요즘 main memory의 크기가 과거에 비해 비약적으로 늘었다고 하지만 여전히 memory는 한정된 자원이라는 것을 명심해야 한다. (2)의 문제는 예상을 초과하는 memory가 필요하게 된다면 에러가 발생하거나, application을 종료시킨 후 memory를 더 크게 할당해주고 다시 컴파일 후 application 재실행을 해줘야 한다. software engineer의 생산성이 떨어지고, application 사용자의 눈은 뒤집어진다.

그렇다면 dynamic memory allocator는 이 문제를 어떻게 해결해줄까? 이름 그대로, runtime에 application의 요청에 의해 필요로 하는 만큼의 메모리를 동적으로 할당해준다. 동적으로 할당해준다는 의미는 application의 요청에 의해 메모리를 할당해주기도 하고, 할당했던 메모리의 크기를 늘였다 줄였다도 하고, 메모리를 더이상 쓸 일이 없어졌을 때는 아예 시스템에 반납도 할 수 있게 해준다는 의미다. 이 모든 게 runtime에 이뤄질 수 있게 해주는 것이 가장 큰 특징이다.

## 잠깐, Dynamic memory allocator는 어디에서 memory를 가져와서 할당해주고, 반납해준다는 것인가?
Application이 사용하는 메모리 영역은 여러 `segment`로 범주화되어 관리된다. 크게 text segment, data segment, bss segment, heap segment, stack segment로 나뉜다. 이 heap segment는 다양한 사이즈의 free block과 allocated block들로 이루어져있다. application이 dynamic memory allocator에게 메모리 할당을 요청하면, 적당한 free block을 찾아서 이 free block에 대한 pointer를 application에게 반환해주고 해당 free block을 allocated 되었다고 기입해둔다. 만약 application이 요청하는 사이즈의 free block을 찾을 수 없다면, sbrk라는 system call을 통해서 시스템으로부터 더 많은 메모리를 할당 받는다. 이를 통해 적절한 사이즈의 free block을 확보하고, 이에 대한 포인터를 application에게 반환해준다. 참고로 sbrk를 호출하면 아래 그림과 같이 brk pointer를 위로 올리거나 낮춤으로써 heap memory를 증가시키거나 감소시킬 수 있다.
</br>
![image](https://user-images.githubusercontent.com/61924861/146184395-945453f6-af8e-4586-bef7-ff360dced6c7.png)
</br>
출처: CSAPP 3rd

## Dynamic memory allocator가 대충 어떤 역할을 하는지 알겠다. 그렇다면 실제로 구현을 할 때 어떤 것들을 중요하게 고려해야 하는가?
Dynamic memory allocator 성능을 측정하는 대표적인 지표는 `memory utilization`과 `throughput`이다. memory utilization은 쉽게 말해 heap segment를 낭비하는 공간 없이 application이 필요한 데이터를 얼마나 꽉 채워서 썼는가이다. throughput은 쉽게 말해 얼마나 빨리 application의 allocate/free 요청을 처리해줄 수 있는가이다. 이 두 성능 지표는 trade-off 관계에 있기 때문에 적절히 밸런스를 맞추는 것이 중요하다. Trade-off 관계에 있다고 하면 왜 그런지 바로 공감이 안 될 수 있어서 예시를 하나 들어보겠다. throughput을 높이기 위해서 dynamic memory allocator는 free block이나 allocated block 여러 bookkeeping 정보를 담을 수 있다. 이들 정보를 적절히 활용하여 allocator는 allocate/free 요청을 빠르게 처리할 수 있게 된다. 하지만 이들 overhead 정보는 위에서 application이 저장하고 싶은 정보가 아니기 때문에 언급한 '낭비 공간'에 해당하고, 따라서 memory utilization을 갉아 먹는다.

memory utilization과 throughput에 영향을 끼치는 중요한 구현요소로는 다음과 같은 것들이 있다:

1. free block organization: 
	- 위에서 언급했듯이 heap segment는 array of bytes이고 이 array가 다양한 size의 free block과 allocated block으로 뒤섞여 채워져있다. 이런 상황에서 어떤 방식으로 free block들을 추적 관리할 것인가에 관한 것이 free block organization이다. free block들을 일종의 list 형태로 추적 관리하고 크게 3가지 방식이 있는데, implicit free list, explicit free list, segregated free list가 있다. `어떤 종류의 free block organization을 채택하느냐에 따라 free block format, placing 방식, splitting 방식, coalescing 방식이 모두 달라질 수 있다.`

1. placing:
	- application으로부터 memory를 할당해달라는 요청을 받았을 때, dynamic memory allocator는 위에서 결정된 free block organization에 따라 사용하고 있는 free list에서 적절한 free block을 찾아야 한다. 이때 이때 이 free block을 찾는 방식에 관한 것이 placing이다. 적절한(fit) free block을 찾는 방식으로 크게 3가지 방식이 있는데, first-fit, next-fit, best-fit이 있다.

1. splitting:
	- fit한 free block을 찾고나서 해당 블록을 allocated로 표식해놓고 application한테 해당 block에 대한 pointer를 바로 반환할 수도 있다. 하지만 만약 해당 free block이 application이 요청한 사이즈보다 훨씬 클 경우 이 block을 통째로 application에게 주면 낭비가 되지 않을까(internal fragmentation)? 이 낭비를 줄이기 위해서, free block을 적절히 2개의 block으로 쪼개서 필요한 만큼만 allocated 처리하고 나머지 block은 free 상태로 처리해놓을 수 있다.

1. coalescing:
	- splitting 이후 생성된 free block의 앞 뒤로 만약 또다른 free block이 존재한다면 해당 블록들을 합쳐놓는 게 좋을 수 있다. External fragmentation을 막기 위함이다. 만약 heap에 크기가 4인 free block이 2개가 contiguous하게 존재하는데, applicaiton이 memory 할당을 요청한 사이즈가 5라면 free list에서 fit한 크기의 free block을 찾지 못해 NULL을 반환하거나 sbrk 시스템 콜을 이용해서 heap을 불필요하게 늘려야 할 수도 있다. 이런 경우를 막기 위해 연속적으로 존재하는 free block을 미리 합쳐놓는 작업을 해둘 수도 있는데, 이를 coalescing이라고 한다.

\* Internal fragmentation, external fragmentation 얘기가 자주 나오니, 한 번 간단히 정리하고 넘어가자.
fragmentation 자체의 의미는 사용가능한 메모리가 여기저기 쪼개져 있어 제대로 활용할 수 없는 현상을 말한다. 당연히 memory utilization을 극대화하는 데 방해 요소가 된다.
Internal fragmentation은 파편화된 메모리 쪼가리가 allocated된 block 내부에 있어 활용할 수 없는 경우를 말한다. 예를 들어, 크기가 1000 bytes 짜리 allocated block이 있는데 실질적인 payload는 4 bytes이고 나머지는 bookkeping 정보와 padding으로 채워져있다면 memory allocator 입장에서 bookkeeping 정보와 padding 만큼 사용가능한 메모리가 파편화되어 있다고 볼 수 있다.
External fragmentation은 free block들이 작은 사이즈로 여기저기 파편화되어 존재하는 경우를 말한다. 하나의 큰 free block으로 존재하지 않고, 작은 사이즈로 다수의 free block들이 존재하게 되면, free block들을 합쳤을 때의 크기가 application이 요구하는 사이즈의 블록을 내어줄 수도 있지만 파편화되어 존재해서 내어주지 못하는 상황이 있을 수 있다.

## Free block organization 구현 방식에 대해서 좀 더 자세히 알아보도록 하자. 
코드의 라인 바이 라인 설명은 implicit free list 구현의 경우 [내 깃헙 레포 mm.c](https://github.com/cjy13753/malloc-lab/blob/main/mm.c) 파일에, segregated free list 구현의 경우 [mm_segre.c](https://github.com/cjy13753/malloc-lab/blob/main/mm_segre.c)에 주석으로 자세히 달아놨으니, 큰 개념만 정리하는 것을 목표로 한다.
- implicit free list organization
	- Implicit의 의미를 짚고 넘어가는 것이 중요하다. implicit free list organization에서는 free block list에 있는 free block들 간의 관계가 명시적으로 나타나있지 않고 `암묵적`으로 나타난다. 구현에 따라 다르지만 보통 implicit free list에서는 free block의 header에 각 free block의 사이즈와 allocated 여부 정보가 담긴다. block 크기 정보를 바탕으로 포인터의 위치를 옮겨 다음 블록을 보고, 그 블록이 allocated 되었는지 확인하는 식으로 다음 free block을 찾아간다. explicit free list와 대조해서 보면 이 특징이 더 잘보인다. explicit free list organization에서는 각 free block에 사이즈 정보뿐만 아니라, 이전과 다음 free block에 대한 포인터를 `명시적`으로 가지고 있어서, 중간의 allocated block을 건너뛰고 한 번에 다음 free block으로 넘어갈 수 있다.
	- Implicit free list organization에서 memory allocate 요청에 대응하는 방식에 대해 정리해보자. 메모리 할당은 free block을 먼저 찾는 것에서 시작한다. free block을 찾는 방식으로는 크게 3가지가 있다: `first-fit`, `best-fit`, `next-fit`. Implicit free list에서의 `first-fit`은 heap의 처음부터 뒤지기 시작해서, 처음으로 발견하는 free block을 할당해주는 것을 의미한다. heap의 처음부터 뒤지기 때문에 만약 heap의 초반에 크기가 작은 free block들만 많이 남아있다면, 탐색 과정에서 큰 비효율이 발생해 throughput이 떨어질 수 있다. 또한 application으로부터 요청 받은 메모리 할당 사이즈보다 탐색 과정에서 찾은 free block이 훨씬 크더라도 이를 무시하기 때문에, 할당 후 split을 통해 남은 공간을 다시 free block list에 돌려준다 하더라도, external fragmentation으로 이어질 수 있다. 이런 external fragmentation 문제를 해결하기 위해, `best-fit`을 사용할 수도 있다. best-fit의 경우 힙 전체를 다 뒤져서 application으로부터 요청 받은 사이즈와 가장 크기가 비슷한 free block을 찾아서 할당해주기 때문에 free block이 split 되면서 발생하는 external fragmentation의 정도를 감소시킬 수 있다. 하지만 매번 메모리를 할당할 때마다 힙에 존재하는 모든 block을 다 뒤져야 하기 때문에 throughput이 크게 떨어질 수 있다. 마지막으로 `next-fit`에서는 가장 마지막으로 할당된 블록에 대한 정보를 잘 보관하고 있다가, application의 다음 메모리 할당 요청 시 가장 마지막으로 찾았던 free block의 다음 block부터 탐색해서 최초로 발견되는 block을 할당해주는 방식이다. 이 방식은 힙의 초반에 크기가 작은 free block들이 널부러져 있고, 힙의 후반으로 갈수록 크기가 널럴할 free block이 많을 확률이 높은 점을 적극 활용하는 방식이다. first-fit 방식에 비해 throughput을 향상시킬 수 있다.
- segregated free list organization
	- 다수의 explicit free list(링크드리스트)가 사이즈군(size class) 별로 구분되어(segregated) 존재하는 방식이다. size class를 정의하는 방식은 매우 다양할 수 있는데, 책에서 소개하는 방식은 2의 지수함수로 분류한다. 예를 들어, 사이즈가 1~2면 class 0, 사이즈가 3\~4면 class 1, 5\~8이면 class 2, 9\~15이면 class 3, 16\~32면 class 4...와 같은 식이다. 이렇게 size class를 나눌 때의 장점을 와닿게 이해하기 위해 first-fit 방식으로 메모리 할당을 하는 예시를 들겠다. 만약 application이 요구하는 메모리 할당 사이즈가 n이라고 하자. size class가 2의 지수함수로 구분되어 있기 때문에 O(logn)의 속도로 어떤 size class의 explicit free list에 속하는지를 찾을 수 있다. 그리고 그 explicit free list에 속하는 free block들 중 가장 처음에 존재(first-fit)하는 free block을 application에 할당해준다. 이렇게 할 경우 throughput과 memory utilization 차원에서 큰 개선이 가능해진다. Implicit free list에서는 fit block을 찾는 방식이 무엇이든 탐색의 time complexity는 O(n)이 된다. 하지만 segregated free list에서는 O(logn)으로 크게 감소된다. 또한 size class별로 구분해서 할당해주기 때문에 first-fit으로 탐색하면서도 implicit free list에서의 best-fit 방식과 유사한 memory utilization 성능을 나타낼 수 있다. 실제로 production-level dynamic memory allocator에는 segregated free list 방식이 많이 적용된다고 한다.

## 특기할 만한 Minor details 
### CSAPP3rd Malloc-lab에서 gcc -m32로 컴파일 하는 이유는?
실습 파일을 보면 포인터 자료형 사이즈가 4 bytes인 걸 전제하고 포인터 관련 작업들이 수행된다. gcc의 default인 64 bit mode로 컴파일할 경우, 포인터 사이즈가 8 bytes가 되기 때문에 제대로 작동할 수 없다. 단적인 예로 다음 매크로를 보자.
``` c
#define PRED(bp) (*(char **)(bp))
``` 
bp(block pointer)를 가지고 predecessor block에 대한 포인터를 얻기 위한 매크로이다. 우선 현재 코드에서는 predecessor pointer가 4 bytes로 저장되어 있다. 위 매크로를 기계적으로 해석하면 bp를 포인터(에 대한 포인터)로 형변환했기 때문에 dereference를 하면 32 bit mode compiler에서는 4 bytes를 읽어들이고, 64 bit mode compiler에서는 8 bytes를 읽어들인다. 따라서 64 bit mode compiler로 우리의 코드를 컴파일해버리면 위의 PRED 매크로가 제대로 동작할 수 없다.
### 우리 교과서에서 block을 8 bytes aligned 한다는 의미와 이유는?
- unused padding과 epilogue header 제외한 모든 block의 크기는 8의 배수이다. 또한  payload를 가리키는 포인터(segregated free list에서는 predecessor pointer)는 아래 그림과 같이 8의 배수를 주소값으로 가진다.![image](https://user-images.githubusercontent.com/61924861/146671409-c7cf85b3-0a79-42a1-9589-44e33607551e.png)</br>출처: CSAPP3rd
- 교과서에 따르면 block aligning을 하는 이유는 `hold any type of data object`를 위함이라고 한다. 사실 거의 모든 데이터 타입은 8바이트가 최고다.

### 32 bit vs. 64 bit processors에서 각각의 bit가 의미하는 바
보통 processor register, data bus, address bus의 size라고 한다.

### heap list에서 first word를 unused padding으로 하는 이유(pdf 880)
(1 word=4bytes 전제) implicit free list에서 prologue block이 1 word-size header와 1 word-size footer로 총 2 words를 차지한다. 만약 그 다음 블록이 온다면, 1 word-size header가 오게 돼서 실제 payload 앞에 3 words만 존재하게 된다. 그러면 payload를 가리키는 pointer의 주소값 12가 되어 8의 배수가 될 수 없다. 이걸 맞춰주기 위해서 1 word size unused padding이 heap의 맨 앞에 붙게 되는 것이다.

### heap 영역 밖 주소값 (pdf883)
아래 extend_heap 함수에서 NEXT_BLKP(bp)를 해버리면 malloc이 할당하던 공간을 벗어나서 문제가 되는 거 아닌가 싶었는데, 생각해보니 문제 없다. malloc이 할당한 공간 밖이긴 하지만, 그 공간에 접근해서 어떤 값을 읽거나 쓰는 게 아니라, 그냥 주소의 값만 보관하고 있는 것이기 때문이다.
``` c
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define HDRP(bp) ((char *)(bp) - WSIZE)

static void *extend_heap(size_t words)
{
/* 생략 */
PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
/* 생략 */
}
```

### next_fit을 구현할 때 segmentation fault가 발생하는 이유
만약 어쩌다 coalesce 함수가 돌아서 last_fit 포인터가 가리키던 블록이 앞 혹은 뒤 block과 coalesce하게 된다면 bp를 갱신해줘야 한다. 이걸 안 하면 last_fit 포인터가 이상한 곳을 가리키게 돼서 segmentation fault가 발생할 수 있다.

### find_fit 과정에서 last_fit은 고려 대상 안 넣으면 `mem_sbrk failed. Ran out of memory...` 뜨는 이유
엣지 케이스가 존재학 때문이다. last_fit만 free block이고, 나머지 block들은 allocated 상태이고, 새로 요구되는 블록의 크기가 last_fit block에 들어갈 수 있는 사이즈일 때 무한으로 extend_heap을 요청하게 될 수 있다.

### pointer를 형변환해주는 이유
- pointer는 내가 읽고 싶은 자료의 맨 앞 바이트 주소값을 보관한다. 포인터가 가리키는 자료 전체를 읽어들이기(dereference) 위해서는 컴파일러에게 맨 앞 바이트 주소부터 시작해서 그 뒤로 얼마만큼의 바이트를 함께 읽어들여야 하는지 알려줘야 한다. 만약 포인터가 가리키는 자료형이 void라면 컴파일러 입장에서는 얼마만큼의 바이트를 읽어들여야 할지 알 수 없다. 즉, 포인터에 대한 dereferencing을 할 수 없다. 따라서 포인터 변수를 적절한 자료형에 대한 포인터로 형변환해줘야 한다.
- 그렇다면 void \*를 왜 쓸까 하는 의문이 들 수 있다. 그냥 char *, int * 등등으로 명시적으로 표현해주면 되지 않는가? 함수가 파라미터로 받아들여야 하는 포인터가 어떤 자료형에 대한 포인터인지 알 수 없을 수 있기 때문이다. void *는 모든 종류의 포인터 자료형을 받을 수 있다. 